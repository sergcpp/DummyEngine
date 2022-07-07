#include "VertexInput.h"

#include "GL.h"

namespace Ren {
const uint32_t g_gl_attrib_types[] = {
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
static_assert(COUNT_OF(g_gl_attrib_types) == size_t(eType::_Count), "!");

bool IsIntegerType(const eType type) { return type == eType::Uint32 || type == eType::Int32 || type == eType::Uint16; }
bool IsNormalizedType(const eType type) {
    return type == eType::Uint16UNorm || type == eType::Int16SNorm || type == eType::Uint8UNorm;
}
} // namespace Ren

Ren::VertexInput::VertexInput() {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    gl_vao_ = uint32_t(vao);
}

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

    gl_vao_ = exchange(rhs.gl_vao_, 0);
    attribs = std::move(rhs.attribs);
    elem_buf = exchange(rhs.elem_buf, {});

    return (*this);
}

bool Ren::VertexInput::Setup(Span<const VtxAttribDesc> _attribs, const BufHandle &_elem_buf) {
    if (_attribs.size() == attribs.size() && std::equal(_attribs.begin(), _attribs.end(), attribs.data()) &&
        _elem_buf == elem_buf) {
        return true;
    }

    glBindVertexArray(GLuint(gl_vao_));

    attribs.clear();

    for (int i = 0; i < _attribs.size(); i++) {
        const VtxAttribDesc &a = _attribs[i];

        glBindBuffer(GL_ARRAY_BUFFER, a.buf.id);

        glEnableVertexAttribArray(GLuint(a.loc));
        if (IsIntegerType(a.type)) {
            glVertexAttribIPointer(GLuint(a.loc), GLint(a.size), g_gl_attrib_types[int(a.type)], GLsizei(a.stride),
                                   reinterpret_cast<void *>(uintptr_t(a.offset)));
        } else {
            glVertexAttribPointer(GLuint(a.loc), GLint(a.size), g_gl_attrib_types[int(a.type)],
                                  IsNormalizedType(a.type) ? GL_TRUE : GL_FALSE, GLsizei(a.stride),
                                  reinterpret_cast<void *>(uintptr_t(a.offset)));
        }

        attribs.emplace_back(a);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLuint(_elem_buf.id));
    elem_buf = _elem_buf;

    glBindVertexArray(0);

    return true;
}