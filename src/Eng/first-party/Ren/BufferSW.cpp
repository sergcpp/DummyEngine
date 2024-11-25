#include "Buffer.h"

#include <cassert>

#include <algorithm>

#if defined(REN_GL_BACKEND)
#include "GL.h"
#elif defined(REN_SW_BACKEND)
#include "SW/SW.h"
#endif

Ren::Buffer::Buffer(std::string_view name, eBufferType type, uint32_t initial_size)
    : name_(name), type_(type), size_(0) {
    nodes_.emplace_back();
    nodes_.back().size = initial_size;

#if defined(REN_GL_BACKEND)
    buf_id_ = 0xffffffff;
#endif

    Resize(initial_size);
}

Ren::Buffer::~Buffer() {
#if defined(REN_GL_BACKEND)
    if (buf_id_ != 0xffffffff) {
        auto gl_buf = (GLuint)buf_id_;
        glDeleteBuffers(1, &gl_buf);
    }
#endif
}

Ren::Buffer &Ren::Buffer::operator=(Buffer &&rhs) noexcept {
    RefCounter::operator=(std::move((RefCounter &)rhs));

    if (buf_id_ == 0xffffffff) {
#if defined(REN_GL_BACKEND)
        auto buf = (GLuint)buf_id_;
        glDeleteBuffers(1, &buf);
#endif
    }

#if defined(REN_GL_BACKEND) || defined(REN_SW_BACKEND)
    buf_id_ = rhs.buf_id_;
    rhs.buf_id_ = 0xffffffff;
#endif

    nodes_ = std::move(rhs.nodes_);

    size_ = rhs.size_;
    rhs.size_ = 0;

    name_ = std::move(rhs.name_);

    return *this;
}

int Ren::Buffer::Alloc_Recursive(int i, uint32_t req_size) {
    if (!nodes_[i].is_free || req_size > nodes_[i].size) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        int new_node = Alloc_Recursive(ch0, req_size);
        if (new_node != -1) return new_node;

        return Alloc_Recursive(ch1, req_size);
    } else {
        if (req_size == nodes_[i].size) {
            nodes_[i].is_free = false;
            return i;
        }

        nodes_[i].child[0] = ch0 = int(nodes_.size());
        nodes_.emplace_back();
        nodes_[i].child[1] = ch1 = int(nodes_.size());
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
    if ((nodes_[i].is_free && !nodes_[i].has_children()) ||
        offset < nodes_[i].offset || offset > (nodes_[i].offset + nodes_[i].size)) {
        return -1;
    }

    int ch0 = nodes_[i].child[0], ch1 = nodes_[i].child[1];

    if (ch0 != -1) {
        int ndx = Find_Recursive(ch0, offset);
        if (ndx != -1) return ndx;
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
    int last = int(nodes_.size()) - 1;

    if (last != i) {
        int ch0 = nodes_[last].child[0],
            ch1 = nodes_[last].child[1];

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
    if (i == -1 || nodes_[i].is_free) return false;

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

uint32_t Ren::Buffer::Alloc(uint32_t req_size, const void *init_data) {
    int i = Alloc_Recursive(0, req_size);
    if (i != -1) {
        Node &n = nodes_[i];
        assert(n.size == req_size);

        if (init_data) {
#if defined(REN_GL_BACKEND)
            glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buf_id_);
            glBufferSubData(GL_ARRAY_BUFFER, n.offset, n.size, init_data);
#elif defined(REN_SW_BACKEND)
            swBindBuffer(SW_ARRAY_BUFFER, (SWuint)buf_id_);
            swBufferSubData(SW_ARRAY_BUFFER, n.offset, n.size, init_data);
#endif
        }
        
        return n.offset;
    } else {
        Resize(size_ + req_size);
        return Alloc(req_size);
    }
}

bool Ren::Buffer::Free(uint32_t offset) {
    int i = Find_Recursive(0, offset);
    return Free_Node(i);
}

void Ren::Buffer::Resize(uint32_t new_size) {
    if (size_ >= new_size) return;

    uint32_t old_size = size_;

    if (!size_) size_ = new_size;

    while (size_ < new_size) {
        size_ *= 2;
    }

#if defined(REN_GL_BACKEND)
    GLuint gl_buffer;
    glGenBuffers(1, &gl_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffer);
    glBufferData(GL_ARRAY_BUFFER, size_, nullptr, GL_STATIC_DRAW);

    if (buf_id_ != 0xffffffff) {
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)buf_id_);
        glBindBuffer(GL_COPY_WRITE_BUFFER, gl_buffer);

        glCopyBufferSubData(GL_ARRAY_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, old_size);

        auto old_buffer = (GLuint)buf_id_;
        glDeleteBuffers(1, &old_buffer);

        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    }

    buf_id_ = (uint32_t)gl_buffer;
#elif defined(REN_SW_BACKEND)
    SWuint sw_buffer = swCreateBuffer();
    swBindBuffer(SW_ARRAY_BUFFER, sw_buffer);
    swBufferData(SW_ARRAY_BUFFER, size_, nullptr);

    if (buf_id_ != 0xffffffff) {
        swBindBuffer(SW_ARRAY_BUFFER, (SWuint)buf_id_);

        void *_temp = malloc(old_size);
        swGetBufferSubData(SW_ARRAY_BUFFER, 0, old_size, _temp);

        swBindBuffer(SW_ARRAY_BUFFER, sw_buffer);
        swBufferSubData(SW_ARRAY_BUFFER, 0, old_size, _temp);

        free(_temp);
    }

    buf_id_ = (uint32_t)sw_buffer;
#endif
}