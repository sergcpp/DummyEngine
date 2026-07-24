#include "../Buffer.h"

#include <algorithm>
#include <cassert>

#include "../Common.h"
#include "../Config.h"
#include "../Log.h"
#include "GL.h"
#include "GLCtx.h"

namespace Ren {
const uint32_t g_buf_targets_gl[] = {
    0xffffffff,               // Undefined
    GL_ARRAY_BUFFER,          // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER,  // VertexIndices
    GL_TEXTURE_BUFFER,        // Texture
    GL_UNIFORM_BUFFER,        // Uniform
    GL_SHADER_STORAGE_BUFFER, // Storage
    GL_COPY_WRITE_BUFFER,     // Stage
    GL_COPY_READ_BUFFER,      // Stage
    0xffffffff,               // AccStructure
    0xffffffff,               // ShaderBinding
    GL_DRAW_INDIRECT_BUFFER   // Indirect
};
static_assert(std::size(g_buf_targets_gl) == size_t(eBufType::_Count));

GLenum GetGLBufUsage(const eBufType type) {
    if (type == eBufType::Upload || type == eBufType::Readback) {
        return GL_STREAM_COPY;
    } else {
        return GL_STATIC_DRAW;
    }
}

#if !defined(__ANDROID__)
GLbitfield GetGLBufStorageFlags(const eBufType type) {
    GLbitfield flags = GL_DYNAMIC_STORAGE_BIT;

    if (type == eBufType::Upload) {
        flags |= (GL_CLIENT_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT);
    } else if (type == eBufType::Readback) {
        flags |= (GL_CLIENT_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT | GL_MAP_COHERENT_BIT);
    }

    return flags;
}
#endif

uint32_t GLInternalFormatFromFormat(eFormat format);
} // namespace Ren

bool Ren::Buffer_Init(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, String name,
                      const eBufType type, const uint32_t initial_size, ILog *log, const uint32_t size_alignment,
                      MemAllocators *mem_allocs) {
    buf_cold.name = std::move(name);
    buf_cold.type = type;
    buf_cold.size = 0;
    buf_cold.size_alignment = size_alignment;

    return Buffer_Resize(api, buf_main, buf_cold, initial_size, log);
}

bool Ren::Buffer_Init(const ApiContext &api, BufferCold &buf_cold, String name, const eBufType type,
                      MemAllocation &&alloc, const uint32_t initial_size, ILog *log, const uint32_t size_alignment) {
    buf_cold.name = std::move(name);
    buf_cold.type = type;
    buf_cold.size = initial_size;
    buf_cold.size_alignment = size_alignment;

    return true;
}

void Ren::Buffer_Destroy(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    assert(buf_cold.mapped_offset == 0xffffffff && !buf_cold.mapped_ptr);
    if (buf_main.buf) {
        auto gl_buf = GLuint(buf_main.buf);
        glDeleteBuffers(1, &gl_buf);
        for (const auto view : buf_main.views) {
            auto gl_tex = GLuint(view.second);
            glDeleteTextures(1, &gl_tex);
        }
    }
    buf_main = {};
    buf_cold = {};
}

void Ren::Buffer_DestroyImmediately(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    Buffer_Destroy(api, buf_main, buf_cold);
}

bool Ren::Buffer_Resize(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, uint32_t new_size, ILog *log,
                        const bool keep_content, const bool release_immediately) {
    new_size = RoundUp(new_size, buf_cold.size_alignment);
    if (buf_cold.size == new_size) {
        return true;
    }

    const uint32_t old_size = buf_cold.size;

    buf_cold.size = new_size;
    assert(buf_cold.size > 0);

    if (buf_cold.sub_alloc) {
        buf_cold.sub_alloc->ResizePool(0, buf_cold.size);
        assert(buf_cold.sub_alloc->IntegrityCheck());
    }

    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], gl_buffer);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_BUFFER, gl_buffer, -1, buf_cold.name.c_str());
#endif
    glBufferStorage(g_buf_targets_gl[int(buf_cold.type)], buf_cold.size, nullptr, GetGLBufStorageFlags(buf_cold.type));

    auto views = std::move(buf_main.views);

    if (buf_main.buf) {
        glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], GLuint(buf_main.buf));
        glBindBuffer(GL_COPY_WRITE_BUFFER, gl_buffer);

        if (keep_content) {
            glCopyBufferSubData(g_buf_targets_gl[int(buf_cold.type)], GL_COPY_WRITE_BUFFER, 0, 0, old_size);
        }

        auto old_buffer = GLuint(buf_main.buf);
        glDeleteBuffers(1, &old_buffer);
        for (const auto view : views) {
            auto gl_tex = GLuint(view.second);
            glDeleteTextures(1, &gl_tex);
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    buf_main.buf = uint32_t(gl_buffer);
    for (auto view : views) {
        Buffer_AddView(api, buf_main, buf_cold, view.first);
    }

    return true;
}

int Ren::Buffer_AddView(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, const eFormat format) {
    GLuint tex_id;
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &tex_id);
#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_TEXTURE, tex_id, -1, buf_cold.name.c_str());
#endif
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);
    glTexBufferRange(GL_TEXTURE_BUFFER, GLInternalFormatFromFormat(format), GLuint(buf_main.buf), 0, buf_cold.size);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

    buf_main.views.emplace_back(format, tex_id);
    return int(buf_main.views.size()) - 1;
}

uint8_t *Ren::Buffer_MapRange(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold, const uint32_t offset,
                              const uint32_t size, const bool persistent) {
    assert(buf_cold.mapped_offset == 0xffffffff && !buf_cold.mapped_ptr);
    assert(offset + size <= buf_cold.size);

    GLbitfield buf_map_range_flags = GLbitfield(GL_MAP_COHERENT_BIT);

    if (persistent) {
        buf_map_range_flags |= GLbitfield(GL_MAP_PERSISTENT_BIT);
    }

    if (buf_cold.type == eBufType::Upload) {
        buf_map_range_flags |= GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_WRITE_BIT) |
                               GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT);
    } else if (buf_cold.type == eBufType::Readback) {
        buf_map_range_flags |= GLbitfield(GL_MAP_READ_BIT);
    }

    glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], GLuint(buf_main.buf));
    auto *ret = (uint8_t *)glMapBufferRange(g_buf_targets_gl[int(buf_cold.type)], GLintptr(offset), GLsizeiptr(size),
                                            buf_map_range_flags);
    glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], GLuint(0));

    buf_cold.mapped_offset = offset;
    buf_cold.mapped_ptr = ret;

    return ret;
}

void Ren::Buffer_Unmap(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold) {
    assert(buf_cold.mapped_offset != 0xffffffff && buf_cold.mapped_ptr);
    glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], GLuint(buf_main.buf));
    glUnmapBuffer(g_buf_targets_gl[int(buf_cold.type)]);
    glBindBuffer(g_buf_targets_gl[int(buf_cold.type)], GLuint(0));
    buf_cold.mapped_offset = 0xffffffff;
    buf_cold.mapped_ptr = nullptr;
}

Ren::SubAllocation Ren::Buffer_AllocSubRegion(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold,
                                              const uint32_t req_size, const uint32_t req_alignment,
                                              std::string_view tag, ILog *log, const BufferMain *init_buf,
                                              CommandBuffer cmd_buf, const uint32_t init_off) {
    if (!buf_cold.sub_alloc) {
        buf_cold.sub_alloc = std::make_unique<FreelistAlloc>(buf_cold.size);
    }

    FreelistAlloc::Allocation alloc = buf_cold.sub_alloc->Alloc(req_alignment, req_size);
    while (alloc.pool == 0xffff) {
        const auto new_size = req_alignment * ((uint32_t(buf_cold.size * 1.25f) + req_alignment - 1) / req_alignment);
        if (!Buffer_Resize(api, buf_main, buf_cold, new_size, log)) {
            return {};
        }
        alloc = buf_cold.sub_alloc->Alloc(req_alignment, req_size);
    }
    assert(alloc.pool == 0);
    assert(buf_cold.sub_alloc->IntegrityCheck());
    const SubAllocation ret = {alloc.offset, alloc.block};
    if (ret.offset != 0xffffffff) {
        if (init_buf) {
            Buffer_UpdateSubRegion(api, buf_main, buf_cold, ret.offset, req_size, *init_buf, init_off);
        }
    }
    return ret;
}

void Ren::Buffer_UpdateSubRegion(const ApiContext &api, BufferMain &buf_main, BufferCold &buf_cold,
                                 const uint32_t offset, const uint32_t size, const BufferMain &init_buf,
                                 const uint32_t init_off, CommandBuffer cmd_buf) {
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(init_buf.buf));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(buf_main.buf));

    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, GLintptr(init_off), GLintptr(offset),
                        GLsizeiptr(size));

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

bool Ren::Buffer_FreeSubRegion(BufferCold &buf_cold, SubAllocation alloc) {
    buf_cold.sub_alloc->Free(alloc.block);
    assert(buf_cold.sub_alloc->IntegrityCheck());
    return true;
}

void Ren::Buffer_Fill(const ApiContext &, BufferMain &buf_main, const uint32_t dst_offset, const uint32_t size,
                      const uint32_t data, CommandBuffer) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(buf_main.buf));
    glClearBufferSubData(GL_COPY_WRITE_BUFFER, GL_R32UI, GLintptr(dst_offset), GLsizeiptr(size), GL_RED_INTEGER,
                         GL_UNSIGNED_INT, &data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::Buffer_UpdateInPlace(const ApiContext &, BufferMain &buf_main, const uint32_t dst_offset, const uint32_t size,
                               const void *data, CommandBuffer) {
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(buf_main.buf));
    glBufferSubData(GL_COPY_WRITE_BUFFER, GLintptr(dst_offset), size, data);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::CopyBufferToBuffer(const ApiContext &api, const BufferMain &src, const uint32_t src_offset, BufferMain &dst,
                             const uint32_t dst_offset, const uint32_t size, CommandBuffer) {
    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(src.buf));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(dst.buf));
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::CopyBufferToBuffer(const ApiContext &api, const StoragesRef &storages, const BufferROHandle src,
                             const uint32_t src_offset, const BufferHandle dst, const uint32_t dst_offset,
                             const uint32_t size, CommandBuffer cmd_buf) {
    const auto &[src_main, src_cold] = storages.buffers[src];
    const auto &[dst_main, dst_cold] = storages.buffers[dst];

    glBindBuffer(GL_COPY_READ_BUFFER, GLuint(src_main.buf));
    glBindBuffer(GL_COPY_WRITE_BUFFER, GLuint(dst_main.buf));
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, size);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void Ren::GLUnbindBufferUnits(const int start, const int count) {
    for (int i = start; i < start + count; i++) {
        glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
        if (i < 16) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, 0);
        }
    }
}
