#include "ProgramGL.h"

#include "GL.h"
#include "Log.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
GLuint LoadShader(GLenum shader_type, const char *source, ILog *log);
#ifndef __ANDROID__
GLuint LoadShader(GLenum shader_type, const uint8_t *data, int data_size, ILog *log);
#endif

void ParseGLSLBindings(const char *shader_str, Descr **bindings, int *bindings_count, ILog *log);
} // namespace Ren

Ren::Program::Program(std::string_view name, ApiContext *api_ctx, ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
                      ShaderRef tes_ref, ShaderRef gs_ref, eProgLoadStatus *status, ILog *log) {
    name_ = String{name};
    Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref), std::move(tes_ref), std::move(gs_ref), status, log);
}

Ren::Program::Program(std::string_view name, ApiContext *api_ctx, ShaderRef cs_ref, eProgLoadStatus *status,
                      ILog *log) {
    name_ = String{name};
    Init(std::move(cs_ref), status, log);
}

Ren::Program::~Program() {
    if (id_) {
        auto prog = GLuint(id_);
        glDeleteProgram(prog);
    }
}

Ren::Program &Ren::Program::operator=(Program &&rhs) noexcept {
    if (this == &rhs) {
        return (*this);
    }

    RefCounter::operator=(static_cast<RefCounter &&>(rhs));

    if (id_) {
        auto prog = GLuint(id_);
        glDeleteProgram(prog);
    }

    id_ = std::exchange(rhs.id_, 0);
    shaders_ = std::move(rhs.shaders_);
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    uniform_blocks_ = std::move(rhs.uniform_blocks_);
    name_ = std::move(rhs.name_);

    return *this;
}

void Ren::Program::Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref, ShaderRef tes_ref, ShaderRef gs_ref,
                        eProgLoadStatus *status, ILog *log) {
    assert(id_ == 0);

    if (!vs_ref || !fs_ref) {
        (*status) = eProgLoadStatus::SetToDefault;
        return;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, (GLuint)vs_ref->id());
        glAttachShader(program, (GLuint)fs_ref->id());
        if (tcs_ref && tes_ref) {
            glAttachShader(program, (GLuint)tcs_ref->id());
            glAttachShader(program, (GLuint)tes_ref->id());
        }
        if (gs_ref) {
            glAttachShader(program, gs_ref->id());
        }
        glLinkProgram(program);
        GLint link_status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE) {
            GLint buf_len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
            if (buf_len) {
                std::unique_ptr<char[]> buf(new char[buf_len]);
                if (buf) {
                    glGetProgramInfoLog(program, buf_len, nullptr, buf.get());
                    log->Error("Could not link program: %s", buf.get());
                }
            }
            glDeleteProgram(program);
            program = 0;
        } else {
#ifdef ENABLE_OBJ_LABELS
            glObjectLabel(GL_PROGRAM, program, -1, name_.c_str());
#endif
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    id_ = uint32_t(program);
    // store shaders
    shaders_[int(eShaderType::Vertex)] = std::move(vs_ref);
    shaders_[int(eShaderType::Fragment)] = std::move(fs_ref);
    shaders_[int(eShaderType::TesselationControl)] = std::move(tcs_ref);
    shaders_[int(eShaderType::TesselationEvaluation)] = std::move(tes_ref);

    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

void Ren::Program::Init(ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    assert(id_ == 0);

    if (!cs_ref) {
        (*status) = eProgLoadStatus::SetToDefault;
        return;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, (GLuint)cs_ref->id());
        glLinkProgram(program);
        GLint link_status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE) {
            GLint buf_len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
            if (buf_len) {
                std::unique_ptr<char[]> buf(new char[buf_len]);
                if (buf) {
                    glGetProgramInfoLog(program, buf_len, nullptr, buf.get());
                    log->Error("Could not link program: %s", buf.get());
                }
            }
            glDeleteProgram(program);
            program = 0;
        } else {
#ifdef ENABLE_OBJ_LABELS
            glObjectLabel(GL_PROGRAM, program, -1, name_.c_str());
#endif
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    id_ = uint32_t(program);
    // store shader
    shaders_[int(eShaderType::Compute)] = std::move(cs_ref);

    InitBindings(log);

    (*status) = eProgLoadStatus::CreatedFromData;
}

void Ren::Program::InitBindings(ILog *log) {
    for (const ShaderRef &sh_ref : shaders_) {
        if (!sh_ref) {
            continue;
        }

        const Shader &sh = (*sh_ref);
        for (const Descr &b : sh.blck_bindings) {
            if (uniform_blocks_.size() < b.loc + 1) {
                uniform_blocks_.resize(b.loc + 1);
            }
            Descr &u = uniform_blocks_[b.loc];
            u.name = b.name;

            if (!b.name.empty() && sh.source() == eShaderSource::GLSL) {
                u.loc = glGetUniformBlockIndex(GLuint(id_), b.name.c_str());
                if (u.loc != -1) {
                    glUniformBlockBinding(GLuint(id_), u.loc, b.loc);
                }
            }
        }
    }

    // Enumerate attributes
    GLint num;
    glGetProgramiv(GLuint(id_), GL_ACTIVE_ATTRIBUTES, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveAttrib(GLuint(id_), i, 128, &len, &len, &n, name);

        Descr &new_attr = attributes_.emplace_back();
        new_attr.name = String{name};
        new_attr.loc = glGetAttribLocation(GLuint(id_), name);
    }

#if 0
    log->Info("PROGRAM %s", name_.c_str());

    // Print all attributes
    log->Info("\tATTRIBUTES");
    for (int i = 0; i < int(attributes_.size()); i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", attributes_[i].name.c_str(), attributes_[i].loc);
    }
#endif

    // Enumerate uniforms
    glGetProgramiv(GLuint(id_), GL_ACTIVE_UNIFORMS, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveUniform(GLuint(id_), i, 128, &len, &len, &n, name);

        Descr &new_uniform = uniforms_.emplace_back();
        new_uniform.name = String{name};
        new_uniform.loc = glGetUniformLocation(GLuint(id_), name);
    }

#if 0
    // Print all uniforms
    log->Info("\tUNIFORMS");
    for (int i = 0; i < int(uniforms_.size()); i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }
#endif
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
