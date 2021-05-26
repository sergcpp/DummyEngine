#include "Buffer.h"

#include <algorithm>
#include <cassert>

#include "GL.h"
#include "Log.h"

namespace Ren {
const uint32_t g_gl_buf_targets[] = {
    0xffffffff,              // Undefined
    GL_ARRAY_BUFFER,         // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER, // VertexIndices
    GL_TEXTURE_BUFFER,       // Texture
    GL_UNIFORM_BUFFER,       // Uniform
    GL_SHADER_STORAGE_BUFFER // Storage
};
static_assert(sizeof(g_gl_buf_targets) / sizeof(g_gl_buf_targets[0]) ==
                  size_t(eBufferType::_Count),
              "!");

GLenum GetGLBufUsage(const eBufferAccessType access, const eBufferAccessFreq freq) {
    if (access == eBufferAccessType::Draw) {
        if (freq == eBufferAccessFreq::Stream) {
            return GL_STREAM_DRAW;
        } else if (freq == eBufferAccessFreq::Static) {
            return GL_STATIC_DRAW;
        } else if (freq == eBufferAccessFreq::Dynamic) {
            return GL_DYNAMIC_DRAW;
        } else {
            assert(false);
        }
    } else if (access == eBufferAccessType::Read) {
        if (freq == eBufferAccessFreq::Stream) {
            return GL_STREAM_READ;
        } else if (freq == eBufferAccessFreq::Static) {
            return GL_STATIC_READ;
        } else if (freq == eBufferAccessFreq::Dynamic) {
            return GL_DYNAMIC_READ;
        } else {
            assert(false);
        }
    } else if (access == eBufferAccessType::Copy) {
        if (freq == eBufferAccessFreq::Stream) {
            return GL_STREAM_COPY;
        } else if (freq == eBufferAccessFreq::Static) {
            return GL_STATIC_COPY;
        } else if (freq == eBufferAccessFreq::Dynamic) {
            return GL_DYNAMIC_COPY;
        } else {
            assert(false);
        }
    } else {
        assert(false);
    }
    return 0xffffffff;
}

} // namespace Ren

int Ren::Buffer::g_GenCounter = 0;

Ren::Buffer::Buffer(const char *name, const eBufferType type,
                    const eBufferAccessType access, const eBufferAccessFreq freq,
                    const uint32_t initial_size)
    : name_(name), type_(type), access_(access), freq_(freq), size_(0) {
    nodes_.reserve(1024);

    nodes_.emplace();
    nodes_[0].size = initial_size;

    Resize(initial_size);
}

Ren::Buffer::~Buffer() {
    if (handle_.id) {
        auto gl_buf = GLuint(handle_.id);
        glDeleteBuffers(1, &gl_buf);
    }
}

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(std::move((RefCounter &)rhs));

    if (handle_.id) {
        auto buf = GLuint(handle_.id);
        glDeleteBuffers(1, &buf);
    }

    handle_ = exchange(rhs.handle_, {});
    name_ = std::move(rhs.name_);

    type_ = exchange(rhs.type_, eBufferType::Undefined);

    access_ = rhs.access_;
    freq_ = rhs.freq_;

    size_ = exchange(rhs.size_, 0);
    if (size_ == 0) {
        __debugbreak();
    }

    nodes_ = std::move(rhs.nodes_);

    return (*this);
}

int Ren::Buffer::Alloc_Recursive(const int i, const uint32_t req_size, const char *tag) {
    if (!nodes_[i].is_free || req_size > nodes_[i].size) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int new_node = Alloc_Recursive(ch0, req_size, tag);
        if (new_node != -1) {
            return new_node;
        }

        return Alloc_Recursive(ch1, req_size, tag);
    } else {
        if (req_size == nodes_[i].size) {
#ifndef NDEBUG
            strncpy(nodes_[i].tag, tag, 31);
#endif
            nodes_[i].is_free = false;
            return i;
        }

        nodes_[i].child[0] = ch0 = nodes_.emplace();
        nodes_[i].child[1] = ch1 = nodes_.emplace();

        Node &n = nodes_[i];

        nodes_[ch0].offset = n.offset;
        nodes_[ch0].size = req_size;
        nodes_[ch1].offset = n.offset + req_size;
        nodes_[ch1].size = n.size - req_size;
        nodes_[ch0].parent = nodes_[ch1].parent = i;

        return Alloc_Recursive(ch0, req_size, tag);
    }
}

int Ren::Buffer::Find_Recursive(const int i, const uint32_t offset) const {
    if ((nodes_[i].is_free && !nodes_[i].has_children()) || offset < nodes_[i].offset ||
        offset > (nodes_[i].offset + nodes_[i].size)) {
        return -1;
    }

    const int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int ndx = Find_Recursive(ch0, offset);
        if (ndx != -1) {
            return ndx;
        }
        return Find_Recursive(ch1, offset);
    } else {
        if (offset == nodes_[i].offset) {
            return i;
        } else {
            return -1;
        }
    }
}

bool Ren::Buffer::Free_Node(int i) {
    if (i == -1 || nodes_[i].is_free) {
        return false;
    }

    nodes_[i].is_free = true;

    int par = nodes_[i].parent;
    while (par != -1) {
        int ch0 = nodes_[par].child[0], ch1 = nodes_[par].child[1];

        if (!nodes_[ch0].has_children() && nodes_[ch0].is_free &&
            !nodes_[ch1].has_children() && nodes_[ch1].is_free) {

            nodes_.erase(ch0);
            nodes_.erase(ch1);

            nodes_[par].child[0] = nodes_[par].child[1] = -1;

            i = par;
            par = nodes_[par].parent;
        } else {
            par = -1;
        }
    }

    { // merge empty nodes
        int par = nodes_[i].parent;
        while (par != -1 && nodes_[par].child[0] == i && !nodes_[i].has_children()) {
            int gr_par = nodes_[par].parent;
            if (gr_par != -1 && nodes_[gr_par].has_children()) {
                int ch0 = nodes_[gr_par].child[0], ch1 = nodes_[gr_par].child[1];

                if (!nodes_[ch0].has_children() && nodes_[ch0].is_free && ch1 == par) {
                    assert(nodes_[ch0].offset + nodes_[ch0].size == nodes_[i].offset);
                    nodes_[ch0].size += nodes_[i].size;
                    nodes_[gr_par].child[1] = nodes_[par].child[1];
                    nodes_[nodes_[par].child[1]].parent = gr_par;

                    nodes_.erase(i);
                    nodes_.erase(par);

                    i = ch0;
                    par = gr_par;
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    return true;
}

void Ren::Buffer::PrintNode(int i, std::string prefix, bool is_tail, ILog *log) {
    const auto &node = nodes_[i];
    if (is_tail) {
        if (!node.has_children() && node.is_free) {
            log->Info("%s+- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset,
                      node.offset + node.size);
        } else {
            log->Info("%s+- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset,
                      node.offset + node.size, node.tag);
        }
        prefix += "   ";
    } else {
        if (!node.has_children() && node.is_free) {
            log->Info("%s|- [0x%08x..0x%08x) <free>", prefix.c_str(), node.offset,
                      node.offset + node.size);
        } else {
            log->Info("%s|- [0x%08x..0x%08x) <%s>", prefix.c_str(), node.offset,
                      node.offset + node.size, node.tag);
        }
        prefix += "|  ";
    }

    if (node.child[0] != -1) {
        PrintNode(node.child[0], prefix, false, log);
    }

    if (node.child[1] != -1) {
        PrintNode(node.child[1], prefix, true, log);
    }
}

uint32_t Ren::Buffer::AllocRegion(uint32_t req_size, const char *tag,
                                  const void *init_data) {
    const int i = Alloc_Recursive(0, req_size, tag);
    if (i != -1) {
        Node &n = nodes_[i];
        assert(n.size == req_size);

        if (init_data) {
            glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
            glBufferSubData(g_gl_buf_targets[int(type_)], n.offset, n.size, init_data);
        }

        return n.offset;
    } else {
        assert(false && "Not implemented!");
        Resize(size_ + req_size);
        return AllocRegion(req_size, tag, init_data);
    }
}

bool Ren::Buffer::FreeRegion(uint32_t offset) {
    const int i = Find_Recursive(0, offset);
    return Free_Node(i);
}

void Ren::Buffer::Resize(uint32_t new_size) {
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

    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(g_gl_buf_targets[int(type_)], gl_buffer);
    glBufferData(g_gl_buf_targets[int(type_)], size_, nullptr,
                 GetGLBufUsage(access_, freq_));

    if (handle_.id) {
        glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
        glBindBuffer(GL_COPY_WRITE_BUFFER, gl_buffer);

        glCopyBufferSubData(g_gl_buf_targets[int(type_)], GL_COPY_WRITE_BUFFER, 0, 0,
                            old_size);

        auto old_buffer = GLuint(handle_.id);
        glDeleteBuffers(1, &old_buffer);

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    handle_.id = uint32_t(gl_buffer);
    handle_.generation = g_GenCounter++;
}

uint8_t *Ren::Buffer::MapRange(uint32_t offset, uint32_t size) {
    const GLbitfield BufferRangeBindFlags =
        GLbitfield(GL_MAP_WRITE_BIT) | GLbitfield(GL_MAP_INVALIDATE_RANGE_BIT) |
        GLbitfield(GL_MAP_UNSYNCHRONIZED_BIT) | GLbitfield(GL_MAP_FLUSH_EXPLICIT_BIT);

    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    uint8_t *ret =
        (uint8_t *)glMapBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset),
                                    GLsizeiptr(size), BufferRangeBindFlags);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));

    return ret;
}

void Ren::Buffer::FlushRange(uint32_t offset, uint32_t size) {
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glFlushMappedBufferRange(g_gl_buf_targets[int(type_)], GLintptr(offset),
                             GLsizeiptr(size));
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
}

void Ren::Buffer::Unmap() {
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
    glUnmapBuffer(g_gl_buf_targets[int(type_)]);
    glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(0));
}

void Ren::GLUnbindBufferUnits(int start, int count) {
    for (int i = start; i < start + count; i++) {
        glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i, 0);
    }
}

void Ren::Buffer::Print(ILog *log) {
    log->Info("=================================================================");
    log->Info("Buffer %s, %f MB, %i nodes", name_.c_str(),
              float(size_) / (1024.0f * 1024.0f), int(nodes_.size()));
    PrintNode(0, "", true, log);
    log->Info("=================================================================");
}
