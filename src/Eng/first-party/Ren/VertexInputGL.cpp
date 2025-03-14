#include "VertexInput.h"

#include "GL.h"

namespace Ren {
const uint32_t g_attrib_types_gl[] = {
    0xffffffff,        // Undefined
    GL_HALF_FLOAT,     // Float16
    GL_FLOAT,          // Float32
    GL_UNSIGNED_INT,   // Uint32
    GL_UNSIGNED_SHORT, // Uint16
    GL_UNSIGNED_SHORT, // Uint16UNorm
    GL_SHORT,          // Int16SNorm
    GL_UNSIGNED_BYTE,  // Uint8UNorm
    GL_INT,            // Int32
};
static_assert(std::size(g_attrib_types_gl) == size_t(eType::_Count), "!");

bool IsIntegerType(const eType type) { return type == eType::Uint32 || type == eType::Int32 || type == eType::Uint16; }
bool IsNormalizedType(const eType type) {
    return type == eType::Uint16_unorm || type == eType::Int16_snorm || type == eType::Uint8_unorm;
}
} // namespace Ren

Ren::VertexInput::VertexInput() = default;

Ren::VertexInput::~VertexInput() {
    if (gl_vao_) {
        auto vao = GLuint(gl_vao_);
        glDeleteVertexArrays(1, &vao);
    }
}

Ren::VertexInput &Ren::VertexInput::operator=(VertexInput &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    if (gl_vao_) {
        auto vao = GLuint(gl_vao_);
        glDeleteVertexArrays(1, &vao);
    }

    gl_vao_ = std::exchange(rhs.gl_vao_, 0);
    attribs = std::move(rhs.attribs);
    elem_buf = std::exchange(rhs.elem_buf, {});

    return (*this);
}

uint32_t Ren::VertexInput::GetVAO() const {
    bool changed = false;
    if (elem_buf) {
        changed |= (elem_buf->handle() != elem_buf_handle_);
    }
    for (int i = 0; i < int(attribs.size()); ++i) {
        changed |= (attribs[i].buf->handle() != attribs_buf_handles_[i]);
    }
    if (changed) {
        if (!gl_vao_) {
            GLuint vao;
            glGenVertexArrays(1, &vao);
            gl_vao_ = uint32_t(vao);
        }
        glBindVertexArray(GLuint(gl_vao_));

        for (int i = 0; i < int(attribs.size()); i++) {
            const VtxAttribDesc &a = attribs[i];

            glBindBuffer(GL_ARRAY_BUFFER, a.buf->id());
            glEnableVertexAttribArray(GLuint(a.loc));
            if (IsIntegerType(a.type)) {
                glVertexAttribIPointer(GLuint(a.loc), GLint(a.size), g_attrib_types_gl[int(a.type)], GLsizei(a.stride),
                                       reinterpret_cast<void *>(uintptr_t(a.offset)));
            } else {
                glVertexAttribPointer(GLuint(a.loc), GLint(a.size), g_attrib_types_gl[int(a.type)],
                                      IsNormalizedType(a.type) ? GL_TRUE : GL_FALSE, GLsizei(a.stride),
                                      reinterpret_cast<void *>(uintptr_t(a.offset)));
            }

            attribs_buf_handles_[i] = a.buf->handle();
        }
        if (elem_buf) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLuint(elem_buf->id()));
            elem_buf_handle_ = elem_buf->handle();
        }
        glBindVertexArray(0);
    }
    return gl_vao_;
}

void Ren::VertexInput::Init(Span<const VtxAttribDesc> _attribs, const BufRef &_elem_buf) {
    attribs.assign(std::begin(_attribs), std::end(_attribs));
    elem_buf = _elem_buf;
    attribs_buf_handles_.resize(attribs.size());
}
