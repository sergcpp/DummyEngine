#include "Buffer.h"

#include <algorithm>
#include <cassert>

#include "GL.h"
#include "Log.h"

namespace Ren {
const uint32_t g_gl_buf_targets[] = {
    0xffffffff,               // Undefined
    GL_ARRAY_BUFFER,          // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER,  // VertexIndices
    GL_TEXTURE_BUFFER,        // Texture
    GL_UNIFORM_BUFFER,        // Uniform
    GL_SHADER_STORAGE_BUFFER, // Storage
    GL_COPY_READ_BUFFER,      // Stage
    0xffffffff,               // AccStructure
    0xffffffff,               // ShaderBinding
    GL_DRAW_INDIRECT_BUFFER   // Indirect
};
static_assert(std::size(g_gl_buf_targets) == size_t(eBufType::_Count), "!");

GLenum GetGLBufUsage(const eBufType type) {
    if (type == eBufType::Stage) {
        return GL_STREAM_COPY;
    } else {
        return GL_STATIC_DRAW;
    }
}

#if !defined(__ANDROID__)
GLbitfield GetGLBufStorageFlags(const eBufType type) {
    GLbitfield flags = GL_DYNAMIC_STORAGE_BIT;

    if (type == eBufType::Stage) {
        flags |= (GL_CLIENT_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    }

    return flags;
}
#endif

} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const std::string_view name, ApiContext *api_ctx, const eBufType type, const uint32_t initial_size,
                    const uint32_t suballoc_align)
    : name_(name), api_ctx_(api_ctx), type_(type), size_(0), alloc_(std::make_unique<FreelistAlloc>(initial_size)),
      suballoc_align_(suballoc_align) {
    Resize(initial_size);
}

Ren::Buffer::~Buffer() { Free(); }

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    Free();

    assert(mapped_offset_ == 0xffffffff);
    assert(mapped_ptr_ == nullptr);

    api_ctx_ = std::exchange(rhs.api_ctx_, nullptr);
    handle_ = std::exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);
    alloc_ = std::move(rhs.alloc_);
    suballoc_align_ = std::exchange(rhs.suballoc_align_, 1);
    type_ = std::exchange(rhs.type_, eBufType::Undefined);

    size_ = std::exchange(rhs.size_, 0);
    mapped_ptr_ = std::exchange(rhs.mapped_ptr_, nullptr);
    mapped_offset_ = std::exchange(rhs.mapped_offset_, 0xffffffff);

#ifndef NDEBUG
    flushed_ranges_ = std::move(rhs.flushed_ranges_);
#endif

    return (*this);
}

Ren::SubAllocation Ren::Buffer::AllocSubRegion(const uint32_t req_size, const char *tag, const Buffer *init_buf, void *,
                                               const uint32_t init_off) {
    const FreelistAlloc::Allocation alloc = alloc_->Alloc(suballoc_align_, req_size);
    assert(alloc.pool == 0);
    assert(alloc_->IntegrityCheck());
    const SubAllocation ret = {alloc.offset, alloc.block};
    if (ret.offset != 0xffffffff) {
        if (init_buf) {
            UpdateSubRegion(ret.offset, req_size, *init_buf, init_off);
        }
    }
    return ret;
}

void Ren::Buffer::UpdateSubRegion(const uint32_t offset, const uint32_t size, const Buffer &init_buf,
                                  const uint32_t init_off, void *cmd_buf) {
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(init_buf.handle_.id));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(handle_.id));

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr(init_off), GLintptr(offset),
                        GLsizeiptr(size));

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

bool Ren::Buffer::FreeSubRegion(const SubAllocation alloc) {
    alloc_->Free(alloc.block);
    assert(alloc_->IntegrityCheck());
    return true;
}

void Ren::Buffer::Resize(uint32_t new_size, const bool keep_content) {
    if (size_ >= new_size) {
        return;
    }

    const uint32_t old_size = size_;

    if (!size_) {
        size_ = new_size;
        assert(size_ > 0);
    }

    while (size_ < new_size) {
        size_ *= 2;
    }

    alloc_->ResizePool(0, size_);

    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(g_gl_buf_targets[int(type_)], gl_buffer);
#if !defined(__ANDROID__)
    glBufferStorage(g_gl_buf_targets[int(type_)], size_, nullptr, GetGLBufStorageFlags(type_));
#else
    glBufferData(g_gl_buf_targets[int(type_)], size_, nullptr, GetGLBufUsage(type_));
#endif

    if (handle_.id) {
        glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
        glBindBuffer(GL_COPY_WRITE_BUFFER, gl_buffer);

        if (keep_content) {
            glCopyBufferSubData(g_gl_buf_targets[int(type_)], GL_COPY_WRITE_BUFFER, 0, 0, old_size);
        }

        auto old_buffer = GLuint(handle_.id);
        glDeleteBuffers(1, &old_buffer);

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    handle_.id = uint32_t(gl_buffer);
    handle_.generation = g_GenCounter++;
}

void Ren::Buffer::Free() {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    if (handle_.id) {
        auto gl_buf = GLuint(handle_.id);
        glDeleteBuffers(1, &gl_buf);
        handle_ = {};
        size_ = 0;
    }
}

uint8_t *Ren::Buffer::MapRange(const Bitmask<eBufMap> dir, const uint32_t offset, const uint32_t size, const bool persistent) {
    assert(mapped_offset_ == 0xffffffff && !mapped_ptr_);
    assert(offset + size <= size_);

#ifndef NDEBUG
    for (auto it = std::begin(flushed_ranges_); it != std::end(flushed_ranges_);) {
        if (offset + size > it->range.first && offset < it->range.first + it->range.second) {
            const WaitResult res = it->fence.ClientWaitSync(0);
            assert(res == WaitResult::Success);
            it = flushed_ranges_.erase(it);
        } else {
            ++it;
        }
    }
#endif

    GLbitfield buf_map_range_flags = 0;

    if (persistent) {
        buf_map_range_flags |= GLbitfield(GL_MAP_PERSISTENT_BIT);
    }

    if (dir & eBufMap::Read) {
        buf_map_range_flags |= GLbitfield(GL_MAP_READ_BIT);
    }

    if (dir & eBufMap::Write) {
        buf_map_range_flags |= GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_WRITE_BIT) |
                               GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);
        if ((dir & eBufMap::Read) == 0) {
            // write only case
            buf_map_range_flags |= GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT);
        }
    }

    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    auto *ret = (uint8_t *)glMapBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset), GLsizeiptr(size),
                                            buf_map_range_flags);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));

    mapped_offset_ = offset;
    mapped_ptr_ = ret;

    return ret;
}

void Ren::Buffer::FlushMappedRange(const uint32_t offset, const uint32_t size) {
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glFlushMappedBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset), GLsizeiptr(size));
#ifndef NDEBUG
    flushed_ranges_.emplace_back(std::make_pair(mapped_offset_ + offset, size),
                                 SyncFence{glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)});
#endif
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
}

void Ren::Buffer::Unmap() {
    assert(mapped_offset_ != 0xffffffff && mapped_ptr_);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glUnmapBuffer(g_gl_buf_targets[int(type_)]);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
    mapped_offset_ = 0xffffffff;
    mapped_ptr_ = nullptr;
}

void Ren::Buffer::Fill(const uint32_t dst_offset, const uint32_t size, const uint32_t data, void *_cmd_buf) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(handle_.id));
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, GLintptr(dst_offset), GLsizeiptr(size), GL_RED,
                         GL_UNSIGNED_INT, &data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::Buffer::UpdateImmediate(uint32_t dst_offset, uint32_t size, const void *data, void *_cmd_buf) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(handle_.id));
    glBufferSubData(GL_COPY_WRITE_BUFFER, GLintptr(dst_offset), size, data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::Buffer::Print(ILog *log) {
#if 0
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(), float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
#endif
}

uint32_t Ren::Buffer::AlignMapOffset(const uint32_t offset) { return offset; }

void Ren::CopyBufferToBuffer(Buffer &src, const uint32_t src_offset, Buffer &dst, const uint32_t dst_offset,
                             const uint32_t size, void *cmd_buf) {
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(src.id()));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(dst.id()));
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset /* readOffset */,
                        dst_offset /* writeOffset */, size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::GLUnbindBufferUnits(const int start, const int count) {
    for (int i = start; i < start + count; i++) {
        glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, 0);
    }
}
