#include "VaoGL.h"

#include "GL.h"

namespace Ren {
const uint32_t g_gl_attrib_types[] = {
    GL_HALF_FLOAT,     // Float16
    GL_FLOAT,          // Float32
    GL_UNSIGNED_INT,   // Uint32
    GL_UNSIGNED_SHORT, // Uint16UNorm
    GL_SHORT,          // Int16SNorm
    GL_UNSIGNED_BYTE,  // Uint8UNorm
    GL_INT,            // Int32
};
static_assert(sizeof(g_gl_attrib_types) / sizeof(g_gl_attrib_types[0]) ==
                  size_t(eType::_Count),
              "!");

bool IsIntegerType(const eType type) { return type == eType::Uint32 || type == eType::Int32; }
bool IsNormalizedType(const eType type) {
    return type == eType::Uint16UNorm || type == eType::Int16SNorm ||
           type == eType::Uint8UNorm;
}
} // namespace Ren

Ren::Vao::Vao() {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    id_ = uint32_t(vao);
}

Ren::Vao::~Vao() {
    auto vao = GLuint(id_);
    glDeleteVertexArrays(1, &vao);
}

bool Ren::Vao::Setup(const VtxAttribDesc attribs[], const int attribs_count,
                     BufHandle elem_buf) {
    if (attribs_count == attribs_count_ &&
        std::equal(attribs, attribs + attribs_count, attribs_) && elem_buf == elem_buf_) {
        return true;
    }

    glBindVertexArray(GLuint(id_));

    for (int i = 0; i < attribs_count; i++) {
        const VtxAttribDesc &a = attribs[i];

        glBindBuffer(GL_ARRAY_BUFFER, a.buf.id);

        glEnableVertexAttribArray(GLuint(a.loc));
        if (IsIntegerType(a.type)) {
            glVertexAttribIPointer(GLuint(a.loc), GLint(a.size),
                                   g_gl_attrib_types[int(a.type)], GLsizei(a.stride),
                                   reinterpret_cast<void *>(a.pointer));
        } else {
            glVertexAttribPointer(GLuint(a.loc), GLint(a.size),
                                  g_gl_attrib_types[int(a.type)],
                                  IsNormalizedType(a.type) ? GL_TRUE : GL_FALSE,
                                  GLsizei(a.stride), reinterpret_cast<void *>(a.pointer));
        }

        attribs_[i] = a;
    }
    attribs_count_ = attribs_count;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLuint(elem_buf.id));
    elem_buf_ = elem_buf;

    glBindVertexArray(0);

    return true;
}