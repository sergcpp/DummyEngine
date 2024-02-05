#include "ShaderGL.h"

#include "GL.h"
#include "Log.h"

#include "SPIRV-Reflect/spirv_reflect.h"

namespace Ren {
GLuint LoadShader(GLenum shader_type, const char *source, ILog *log);
#ifndef __ANDROID__
GLuint LoadShader(GLenum shader_type, const uint8_t *data, int data_size, ILog *log);
#endif

void ParseGLSLBindings(const char *shader_str, SmallVectorImpl<Descr> &attr_bindings,
                       SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log);

const GLenum GLShaderTypes[] = {GL_VERTEX_SHADER,
                                GL_FRAGMENT_SHADER,
                                GL_TESS_CONTROL_SHADER,
                                GL_TESS_EVALUATION_SHADER,
                                GL_COMPUTE_SHADER,
                                0xffffffff /* RayGen */,
                                0xffffffff /* ClosestHit */,
                                0xffffffff /* AnyHit */,
                                0xffffffff /* Miss */,
                                0xffffffff /*Intersection*/};
static_assert(std::size(GLShaderTypes) == int(Ren::eShaderType::_Count), "!");
} // namespace Ren

Ren::Shader::Shader(const char *name, ApiContext *api_ctx, const char *shader_src, const eShaderType type,
                    eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(shader_src, type, status, log);
}

#ifndef __ANDROID__
Ren::Shader::Shader(const char *name, ApiContext *api_ctx, const uint8_t *shader_code, const int code_size,
                    const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(shader_code, code_size, type, status, log);
}
#endif

Ren::Shader::~Shader() {
    if (id_) {
        auto id = GLuint(id_);
        glDeleteShader(id);
    }
}

Ren::Shader &Ren::Shader::operator=(Shader &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    if (id_) {
        auto id = GLuint(id_);
        glDeleteShader(id);
    }

    id_ = std::exchange(rhs.id_, 0);
    type_ = rhs.type_;
    source_ = rhs.source_;
    name_ = std::move(rhs.name_);

    attr_bindings = std::move(rhs.attr_bindings);
    unif_bindings = std::move(rhs.unif_bindings);
    blck_bindings = std::move(rhs.blck_bindings);

    return (*this);
}

void Ren::Shader::Init(const char *shader_src, const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    InitFromGLSL(shader_src, type, status, log);
}

#ifndef __ANDROID__
void Ren::Shader::Init(const uint8_t *shader_code, const int code_size, const eShaderType type,
                       eShaderLoadStatus *status, ILog *log) {
    InitFromSPIRV(shader_code, code_size, type, status, log);
}
#endif

void Ren::Shader::InitFromGLSL(const char *shader_src, const eShaderType type, eShaderLoadStatus *status, ILog *log) {
    if (!shader_src) {
        (*status) = eShaderLoadStatus::SetToDefault;
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_src, log);
    if (!id_) {
        (*status) = eShaderLoadStatus::SetToDefault;
        return;
    } else {
#ifdef ENABLE_OBJ_LABELS
        glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif
    }

    source_ = eShaderSource::GLSL;

    ParseGLSLBindings(shader_src, attr_bindings, unif_bindings, blck_bindings, log);

    (*status) = eShaderLoadStatus::CreatedFromData;
}

#ifndef __ANDROID__
void Ren::Shader::InitFromSPIRV(const uint8_t *shader_data, const int data_size, const eShaderType type,
                                eShaderLoadStatus *status, ILog *log) {
    if (!shader_data) {
        (*status) = eShaderLoadStatus::SetToDefault;
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_data, data_size, log);
    if (!id_) {
        (*status) = eShaderLoadStatus::SetToDefault;
        return;
    }

    type_ = type;
    source_ = eShaderSource::SPIRV;

#ifdef ENABLE_OBJ_LABELS
    glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif

    SpvReflectShaderModule module = {};
    const SpvReflectResult res = spvReflectCreateShaderModule(data_size, shader_data, &module);
    assert(res == SPV_REFLECT_RESULT_SUCCESS);

    attr_bindings.clear();
    unif_bindings.clear();
    blck_bindings.clear();

    for (uint32_t i = 0; i < module.input_variable_count; i++) {
        const auto *var = module.input_variables[i];
        if (var->built_in == -1) {
            Descr &new_item = attr_bindings.emplace_back();
            new_item.name = String{var->name};
            new_item.loc = var->location;
        }
    }

    for (uint32_t i = 0; i < module.descriptor_binding_count; i++) {
        const auto &desc = module.descriptor_bindings[i];
        if (desc.descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
            Descr &new_item = blck_bindings.emplace_back();
            new_item.name = String{desc.name};
            new_item.loc = desc.binding;
        } else {
            Descr &new_item = unif_bindings.emplace_back();
            new_item.name = String{desc.name};
            new_item.loc = desc.binding;
        }
    }

    spvReflectDestroyShaderModule(&module);

    (*status) = eShaderLoadStatus::CreatedFromData;
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
GLuint Ren::LoadShader(GLenum shader_type, const uint8_t *data, const int data_size, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, data, static_cast<GLsizei>(data_size));
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

void Ren::ParseGLSLBindings(const char *shader_str, SmallVectorImpl<Descr> &attr_bindings,
                            SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log) {
    const char *delims = " \r\n\t";
    const char *p = strstr(shader_str, "/*");
    const char *q = p ? strpbrk(p + 2, delims) : nullptr;

    SmallVectorImpl<Descr> *cur_bind_target = nullptr;
    for (; p != nullptr && q != nullptr; q = strpbrk(p, delims)) {
        if (p == q) {
            p = q + 1;
            continue;
        }

        std::string item(p, q);
        if (item == "/*") {
            cur_bind_target = nullptr;
        } else if (item == "*/" && cur_bind_target) {
            break;
        } else if (item == "ATTRIBUTES") {
            cur_bind_target = &attr_bindings;
        } else if (item == "UNIFORMS") {
            cur_bind_target = &unif_bindings;
        } else if (item == "UNIFORM_BLOCKS") {
            cur_bind_target = &blck_bindings;
        } else if (item == "PERM") {
            cur_bind_target = nullptr;
        } else if (cur_bind_target) {
            p = q + 1;
            q = strpbrk(p, delims);
            if (*p != ':') {
                log->Error("Error parsing shader!");
            }
            p = q + 1;
            q = strpbrk(p, delims);
            int loc = std::atoi(p);

            Descr &new_item = cur_bind_target->emplace_back();
            new_item.name = String{item.c_str()};
            new_item.loc = loc;
        }

        if (!q) {
            break;
        }
        p = q + 1;
    }
}
