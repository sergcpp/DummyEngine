#include "Buffer.h"

#include <algorithm>
#include <cassert>

#include "GL.h"

namespace Ren {
const uint32_t g_gl_buf_targets[] = {
    0xffffffff,              // Undefined
    GL_ARRAY_BUFFER,         // VertexAttribs
    GL_ELEMENT_ARRAY_BUFFER, // VertexIndices
    GL_TEXTURE_BUFFER,       // Texture
    GL_UNIFORM_BUFFER,       // Uniform
};
static_assert(sizeof(g_gl_buf_targets) / sizeof(g_gl_buf_targets[0]) ==
                  (size_t)eBufferType::_Count,
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

Ren::Buffer::Buffer(const char *name, eBufferType type, eBufferAccessType access,
                    eBufferAccessFreq freq, uint32_t initial_size)
    : name_(name), type_(type), access_(access), freq_(freq), size_(0) {
    nodes_.emplace_back();
    nodes_.back().size = initial_size;

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

    handle_ = rhs.handle_;
    rhs.handle_ = {};

    name_ = std::move(rhs.name_);

    type_ = rhs.type_;
    rhs.type_ = eBufferType::Undefined;

    access_ = rhs.access_;
    freq_ = rhs.freq_;

    size_ = rhs.size_;
    rhs.size_ = 0;

    nodes_ = std::move(rhs.nodes_);

    return (*this);
}

int Ren::Buffer::Alloc_Recursive(int i, uint32_t req_size) {
    if (!nodes_[i].is_free || req_size > nodes_[i].size) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        const int new_node = Alloc_Recursive(ch0, req_size);
        if (new_node != -1) {
            return new_node;
        }

        return Alloc_Recursive(ch1, req_size);
    } else {
        if (req_size == nodes_[i].size) {
            nodes_[i].is_free = false;
            return i;
        }

        nodes_[i].child[0] = ch0 = (int)nodes_.size();
        nodes_.emplace_back();
        nodes_[i].child[1] = ch1 = (int)nodes_.size();
        nodes_.emplace_back();

        Node &n = nodes_[i];

        nodes_[ch0].offset = n.offset;
        nodes_[ch0].size = req_size;
        nodes_[ch1].offset = n.offset + req_size;
        nodes_[ch1].size = n.size - req_size;
        nodes_[ch0].parent = nodes_[ch1].parent = i;

        return Alloc_Recursive(ch0, req_size);
    }
}

int Ren::Buffer::Find_Recursive(int i, uint32_t offset) const {
    if ((nodes_[i].is_free && !nodes_[i].has_children()) || offset < nodes_[i].offset ||
        offset > (nodes_[i].offset + nodes_[i].size)) {
        return -1;
    }

    const int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        int ndx = Find_Recursive(ch0, offset);
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

void Ren::Buffer::SafeErase(int i, int *indices, int num) {
    const int last = (int)nodes_.size() - 1;

    if (last != i) {
        int ch0 = nodes_[last].child[0], ch1 = nodes_[last].child[1];

        if (ch0 != -1 && nodes_[i].parent != last) {
            nodes_[ch0].parent = nodes_[ch1].parent = i;
        }

        int par = nodes_[last].parent;

        if (nodes_[par].child[0] == last) {
            nodes_[par].child[0] = i;
        } else if (nodes_[par].child[1] == last) {
            nodes_[par].child[1] = i;
        }

        nodes_[i] = nodes_[last];
    }

    nodes_.erase(nodes_.begin() + last);

    for (int j = 0; j < num && indices; j++) {
        if (indices[j] == last) {
            indices[j] = i;
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

            SafeErase(ch0, &par, 1);
            ch1 = nodes_[par].child[1];
            SafeErase(ch1, &par, 1);

            nodes_[par].child[0] = nodes_[par].child[1] = -1;

            par = nodes_[par].parent;
        } else {
            par = -1;
        }
    }

    return true;
}

uint32_t Ren::Buffer::AllocRegion(uint32_t req_size, const void *init_data) {
    const int i = Alloc_Recursive(0, req_size);
    if (i != -1) {
        Node &n = nodes_[i];
        assert(n.size == req_size);

        if (init_data) {
            glBindBuffer(g_gl_buf_targets[int(type_)], GLuint(handle_.id));
            glBufferSubData(g_gl_buf_targets[int(type_)], n.offset, n.size, init_data);
        }

        return n.offset;
    } else {
        Resize(size_ + req_size);
        return AllocRegion(req_size);
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
    }

    while (size_ < new_size) {
        size_ *= 2;
    }

    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(g_gl_buf_targets[int(type_)], gl_buffer);
    glBufferData(g_gl_buf_targets[int(type_)], size_, nullptr, GetGLBufUsage(access_, freq_));

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

void Ren::GLUnbindBufferUnits(int start, int count) {
    for (int i = start; i < start + count; i++) {
        glBindBufferBase(GL_UNIFORM_BUFFER, i, 0);
    }
}