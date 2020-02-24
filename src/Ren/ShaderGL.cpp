#include "ShaderGL.h"

#include "GL.h"
#include "Log.h"

#include "SPIRV-Reflect/spirv_reflect.h"

namespace Ren {
GLuint LoadShader(GLenum shader_type, const char *source, ILog *log);
#ifndef __ANDROID__
GLuint LoadShader(GLenum shader_type, const uint8_t *data, int data_size, ILog *log);
#endif

void ParseGLSLBindings(const char *shader_str, Descr **bindings, int *bindings_count,
                       ILog *log);
bool IsMainThread();

const GLenum GLShaderTypes[] = {0xffffffff,
                                GL_VERTEX_SHADER,
                                GL_FRAGMENT_SHADER,
                                GL_TESS_CONTROL_SHADER,
                                GL_TESS_EVALUATION_SHADER,
                                GL_COMPUTE_SHADER};
static_assert(sizeof(GLShaderTypes) / sizeof(GLShaderTypes[0]) ==
                  int(Ren::eShaderType::_Count),
              "!");
} // namespace Ren

Ren::Shader::Shader(const char *name, const char *shader_src, const eShaderType type,
                    eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(shader_src, type, status, log);
}

#ifndef __ANDROID__
Ren::Shader::Shader(const char *name, const uint8_t *shader_data, int data_size,
                    eShaderType type, eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(shader_data, data_size, type, status, log);
}
#endif

Ren::Shader::~Shader() {
    if (id_) {
        assert(IsMainThread());
        auto id = GLuint(id_);
        glDeleteShader(id);
    }
}

Ren::Shader &Ren::Shader::operator=(Shader &&rhs) noexcept {
    if (id_) {
        assert(IsMainThread());
        auto id = GLuint(id_);
        glDeleteShader(id);
    }

    id_ = rhs.id_;
    rhs.id_ = 0;
    type_ = rhs.type_;
    name_ = std::move(rhs.name_);

    for (int j = 0; j < 3; j++) {
        int i = 0;
        for (; i < rhs.bindings_count[j]; i++) {
            bindings[j][i] = std::move(rhs.bindings[j][i]);
        }
        for (; i < bindings_count[j]; i++) {
            bindings[j][i] = {};
        }
        bindings_count[j] = rhs.bindings_count[j];
    }

    RefCounter::operator=(std::move(rhs));

    return (*this);
}

void Ren::Shader::Init(const char *shader_src, const eShaderType type,
                       eShaderLoadStatus *status, ILog *log) {
    assert(IsMainThread());
    InitFromGLSL(shader_src, type, status, log);
}

#ifndef __ANDROID__
void Ren::Shader::Init(const uint8_t *shader_data, const int data_size,
                       const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    assert(IsMainThread());
    InitFromSPIRV(shader_data, data_size, type, status, log);
}
#endif

void Ren::Shader::InitFromGLSL(const char *shader_src, const eShaderType type,
                               eShaderLoadStatus *status, ILog *log) {
    if (!shader_src) {
        if (status) {
            (*status) = eShaderLoadStatus::SetToDefault;
        }
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_src, log);
    if (!id_) {
        if (status) {
            (*status) = eShaderLoadStatus::SetToDefault;
        }
        return;
    } else {
#ifdef ENABLE_OBJ_LABELS
        glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif
    }

    Descr *_bindings[3] = {bindings[0], bindings[1], bindings[2]};
    ParseGLSLBindings(shader_src, _bindings, bindings_count, log);

    if (status) {
        (*status) = eShaderLoadStatus::CreatedFromData;
    }
}

#ifndef __ANDROID__
void Ren::Shader::InitFromSPIRV(const uint8_t *shader_data, const int data_size,
                                const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    if (!shader_data) {
        if (status) {
            (*status) = eShaderLoadStatus::SetToDefault;
        }
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_data, data_size, log);
    if (!id_) {
        if (status) {
            (*status) = eShaderLoadStatus::SetToDefault;
        }
        return;
    }

#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif

    SpvReflectShaderModule module = {};
    const SpvReflectResult res =
        spvReflectCreateShaderModule(data_size, shader_data, &module);
    assert(res == SPV_REFLECT_RESULT_SUCCESS);

    bindings_count[0] = 0;
    for (uint32_t i = 0; i < module.input_variable_count; i++) {
        const auto &var = module.input_variables[i];
        if (var.built_in == -1) {
            const int ndx = bindings_count[0]++;
            bindings[0][ndx].name = String{var.name};
            bindings[0][ndx].loc = var.location;
        }
    }

    bindings_count[1] = 0;
    bindings_count[2] = 0;
    for (uint32_t i = 0; i < module.descriptor_binding_count; i++) {
        const auto &desc = module.descriptor_bindings[i];
        if (desc.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            const int ndx = bindings_count[2]++;
            bindings[2][ndx].name = String{desc.name};
            bindings[2][ndx].loc = desc.binding;
        } else {
            const int ndx = bindings_count[1]++;
            bindings[1][ndx].name = String{desc.name};
            bindings[1][ndx].loc = desc.binding;
        }
    }

    spvReflectDestroyShaderModule(&module);

    if (status) {
        (*status) = eShaderLoadStatus::CreatedFromData;
    }
}
#endif

GLuint Ren::LoadShader(GLenum shader_type, const char *source, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

        if (info_len) {
            char *buf = (char *)malloc((size_t)info_len);
            glGetShaderInfoLog(shader, info_len, nullptr, buf);
            if (compiled) {
                log->Info("%s", buf);
            } else {
                log->Error("Could not compile shader %d: %s", int(shader_type), buf);
            }
            free(buf);
        }

        if (!compiled) {
            glDeleteShader(shader);
            shader = 0;
        }
    } else {
        log->Error("glCreateShader failed");
    }

    return shader;
}

#if !defined(__ANDROID__) && !defined(__APPLE__)
GLuint Ren::LoadShader(GLenum shader_type, const uint8_t *data, const int data_size,
                       ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, data,
                       static_cast<GLsizei>(data_size));
        glSpecializeShader(shader, "main", 0, nullptr, nullptr);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        GLint info_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

        if (info_len) {
            char *buf = (char *)malloc((size_t)info_len);
            glGetShaderInfoLog(shader, info_len, nullptr, buf);
            if (compiled) {
                log->Info("%s", buf);
            } else {
                log->Error("Could not compile shader %d: %s", int(shader_type), buf);
            }
            free(buf);
        }

        if (!compiled) {
            glDeleteShader(shader);
            shader = 0;
        }
    } else {
        log->Error("glCreateShader failed");
    }

    return shader;
}
#endif

void Ren::ParseGLSLBindings(const char *shader_str, Descr **bindings, int *bindings_count,
                            ILog *log) {
    const char *delims = " \r\n\t";
    const char *p = strstr(shader_str, "/*");
    const char *q = p ? strpbrk(p + 2, delims) : nullptr;

    Descr *cur_bind_target = nullptr;
    int *cur_bind_count = nullptr;

    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }

        String item(p, q);
        if (item == "/*") {
            cur_bind_target = nullptr;
        } else if (item == "*/" && cur_bind_target) {
            break;
        } else if (item == "ATTRIBUTES") {
            cur_bind_target = bindings[0];
            cur_bind_count = &bindings_count[0];
        } else if (item == "UNIFORMS") {
            cur_bind_target = bindings[1];
            cur_bind_count = &bindings_count[1];
        } else if (item == "UNIFORM_BLOCKS") {
            cur_bind_target = bindings[2];
            cur_bind_count = &bindings_count[2];
        } else if (cur_bind_target) {
            p = q + 1;
            q = strpbrk(p, delims);
            if (*p != ':') {
                log->Error("Error parsing shader!");
            }
            p = q + 1;
            q = strpbrk(p, delims);
            int loc = std::atoi(p);

            cur_bind_target[*cur_bind_count].name = item;
            cur_bind_target[*cur_bind_count].loc = loc;
            (*cur_bind_count)++;
        }

        if (!q) {
            break;
        }
        p = q + 1;
    }
}