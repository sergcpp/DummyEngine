#include "ShaderGL.h"

#include "Config.h"
#include "GL.h"
#include "Log.h"

#include "SPIRV-Reflect/spirv_reflect.h"

namespace Ren {
GLuint LoadShader(GLenum shader_type, std::string_view source, ILog *log);
GLuint LoadShader(GLenum shader_type, Span<const uint8_t> data, ILog *log);

void ParseGLSLBindings(std::string_view shader_str, SmallVectorImpl<Descr> &attr_bindings,
                       SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log);

const GLenum GLShaderTypes[] = {GL_VERTEX_SHADER,
                                GL_FRAGMENT_SHADER,
                                GL_TESS_CONTROL_SHADER,
                                GL_TESS_EVALUATION_SHADER,
                                GL_GEOMETRY_SHADER,
                                GL_COMPUTE_SHADER,
                                0xffffffff /* RayGen */,
                                0xffffffff /* ClosestHit */,
                                0xffffffff /* AnyHit */,
                                0xffffffff /* Miss */,
                                0xffffffff /*Intersection*/};
static_assert(std::size(GLShaderTypes) == int(eShaderType::_Count), "!");
} // namespace Ren

Ren::Shader::Shader(std::string_view name, ApiContext *api_ctx, std::string_view shader_src, const eShaderType type,
                    ILog *log) {
    name_ = String{name};
    Init(shader_src, type, log);
}

Ren::Shader::Shader(std::string_view name, ApiContext *api_ctx, Span<const uint8_t> shader_code, const eShaderType type,
                    ILog *log) {
    name_ = String{name};
    Init(shader_code, type, log);
}

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

void Ren::Shader::Init(std::string_view shader_src, const eShaderType type, ILog *log) {
    InitFromGLSL(shader_src, type, log);
}

#ifndef __ANDROID__
void Ren::Shader::Init(Span<const uint8_t> shader_code, const eShaderType type, ILog *log) {
    InitFromSPIRV(shader_code, type, log);
}
#endif

void Ren::Shader::InitFromGLSL(std::string_view shader_src, const eShaderType type, ILog *log) {
    if (shader_src.empty()) {
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_src, log);
    if (!id_) {
        return;
    } else {
#ifdef ENABLE_GPU_DEBUG
        glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif
    }

    source_ = eShaderSource::GLSL;

    ParseGLSLBindings(shader_src, attr_bindings, unif_bindings, blck_bindings, log);
}

#ifndef __ANDROID__
void Ren::Shader::InitFromSPIRV(Span<const uint8_t> shader_data, const eShaderType type, ILog *log) {
    if (shader_data.empty()) {
        return;
    }

    assert(id_ == 0);
    id_ = LoadShader(GLShaderTypes[int(type)], shader_data, log);
    if (!id_) {
        return;
    }

    type_ = type;
    source_ = eShaderSource::SPIRV;

#ifdef ENABLE_GPU_DEBUG
    glObjectLabel(GL_SHADER, id_, -1, name_.c_str());
#endif

    SpvReflectShaderModule module = {};
    const SpvReflectResult res = spvReflectCreateShaderModule(shader_data.size(), shader_data.data(), &module);
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
}
#endif

GLuint Ren::LoadShader(GLenum shader_type, std::string_view source, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        const char *src = source.data();
        glShaderSource(shader, 1, &src, nullptr);
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
GLuint Ren::LoadShader(GLenum shader_type, Span<const uint8_t> data, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V_ARB, data.data(), static_cast<GLsizei>(data.size()));
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

void Ren::ParseGLSLBindings(std::string_view shader_str, SmallVectorImpl<Descr> &attr_bindings,
                            SmallVectorImpl<Descr> &unif_bindings, SmallVectorImpl<Descr> &blck_bindings, ILog *log) {
    const char *delims = " \r\n\t";
    const char *p = strstr(shader_str.data(), "/*");
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
