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

void ParseGLSLBindings(const char *shader_str, Descr **bindings, int *bindings_count,
                       ILog *log);
bool IsMainThread();
} // namespace Ren

Ren::Program::Program(const char *name, ShaderRef vs_ref, ShaderRef fs_ref,
                      ShaderRef tcs_ref, ShaderRef tes_ref, eProgLoadStatus *status,
                      ILog *log) {
    name_ = String{name};
    Init(std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref), std::move(tes_ref),
         status, log);
}

Ren::Program::Program(const char *name, ShaderRef cs_ref, eProgLoadStatus *status,
                      ILog *log) {
    name_ = String{name};
    Init(std::move(cs_ref), status, log);
}

Ren::Program::~Program() {
    if (prog_id_) {
        assert(IsMainThread());
        auto prog = (GLuint)prog_id_;
        glDeleteProgram(prog);
    }
}

Ren::Program &Ren::Program::operator=(Program &&rhs) noexcept {
    if (prog_id_) {
        assert(IsMainThread());
        auto prog = (GLuint)prog_id_;
        glDeleteProgram(prog);
    }

    prog_id_ = rhs.prog_id_;
    rhs.prog_id_ = 0;
    shaders_ = std::move(rhs.shaders_);
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    uniform_blocks_ = std::move(rhs.uniform_blocks_);
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return *this;
}

void Ren::Program::Init(ShaderRef vs_ref, ShaderRef fs_ref, ShaderRef tcs_ref,
                        ShaderRef tes_ref, eProgLoadStatus *status, ILog *log) {
    assert(prog_id_ == 0);
    assert(IsMainThread());

    if (!vs_ref || !fs_ref) {
        if (status) {
            (*status) = eProgLoadStatus::SetToDefault;
        }
        return;
    }

    GLuint program = (uint32_t)glCreateProgram();
    if (program) {
        glAttachShader(program, (GLuint)vs_ref->shader_id());
        glAttachShader(program, (GLuint)fs_ref->shader_id());
        if (tcs_ref && tes_ref) {
            glAttachShader(program, (GLuint)tcs_ref->shader_id());
            glAttachShader(program, (GLuint)tes_ref->shader_id());
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
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    prog_id_ = uint32_t(program);
    // store shaders
    shaders_[int(eShaderType::Vert)] = std::move(vs_ref);
    shaders_[int(eShaderType::Frag)] = std::move(fs_ref);
    shaders_[int(eShaderType::Tesc)] = std::move(tcs_ref);
    shaders_[int(eShaderType::Tese)] = std::move(tes_ref);

    InitBindings(log);

    if (status) {
        (*status) = eProgLoadStatus::CreatedFromData;
    }
}

void Ren::Program::Init(ShaderRef cs_ref, eProgLoadStatus *status, ILog *log) {
    assert(prog_id_ == 0);
    assert(IsMainThread());

    if (!cs_ref) {
        if (status) {
            (*status) = eProgLoadStatus::SetToDefault;
        }
        return;
    }

    GLuint program = (uint32_t)glCreateProgram();
    if (program) {
        glAttachShader(program, (GLuint)cs_ref->shader_id());
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
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    prog_id_ = uint32_t(program);
    // store shader
    shaders_[int(eShaderType::Comp)] = std::move(cs_ref);

    InitBindings(log);

    if (status) {
        (*status) = eProgLoadStatus::CreatedFromData;
    }
}

void Ren::Program::InitBindings(ILog *log) {
    for (ShaderRef& sh_ref : shaders_) {
        if (!sh_ref) {
            continue;
        }

        Shader& sh = (*sh_ref);
        for (int i = 0; i < sh.bindings_count[0]; i++) {
            Descr& b = sh.bindings[0][i];
            Attribute& a = attributes_[b.loc];
            a.loc = glGetAttribLocation(GLuint(prog_id_), b.name.c_str());
            if (a.loc != -1) {
                a.name = std::move(b.name);
            }
        }

        for (int i = 0; i < sh.bindings_count[1]; i++) {
            Descr& b = sh.bindings[1][i];
            Attribute& u = uniforms_[b.loc];
            u.loc = glGetUniformLocation(GLuint(prog_id_), b.name.c_str());
            if (u.loc != -1) {
                u.name = std::move(b.name);
            }
        }

        for (int i = 0; i < sh.bindings_count[2]; i++) {
            Descr& b = sh.bindings[2][i];
            Attribute& u = uniform_blocks_[b.loc];
            u.loc = glGetUniformBlockIndex(GLuint(prog_id_), b.name.c_str());
            if (u.loc != -1) {
                u.name = std::move(b.name);
                glUniformBlockBinding(GLuint(prog_id_), u.loc, b.loc);
            }
        }
    }

    // Enumerate rest of attributes
    GLint num;
    glGetProgramiv(GLuint(prog_id_), GL_ACTIVE_ATTRIBUTES, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveAttrib(GLuint(prog_id_), i, 128, &len, &len, &n, name);

        int skip = 0, free_index = -1;
        for (int j = 0; j < MaxAttributesCount; j++) {
            if (free_index == -1 && attributes_[j].loc == -1) {
                free_index = j;
            }
            if (attributes_[j].loc != -1 && name[0] && attributes_[j].name == name) {
                skip = 1;
                break;
            }
        }

        if (!skip && free_index != -1) {
            attributes_[free_index].name = String{ name };
            attributes_[free_index].loc = glGetAttribLocation(GLuint(prog_id_), name);
        }
    }

    log->Info("PROGRAM %s", name_.c_str());

    // Print all attributes
    log->Info("\tATTRIBUTES");
    for (int i = 0; i < MaxAttributesCount; i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", attributes_[i].name.c_str(), attributes_[i].loc);
    }

    // Enumerate rest of uniforms
    glGetProgramiv(GLuint(prog_id_), GL_ACTIVE_UNIFORMS, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveUniform(GLuint(prog_id_), i, 128, &len, &len, &n, name);

        int skip = 0, free_index = -1;
        for (int j = 0; j < MaxUniformsCount; j++) {
            if (free_index == -1 && uniforms_[j].loc == -1) {
                free_index = j;
            }
            if (uniforms_[j].loc != -1 && uniforms_[j].name == name) {
                skip = 1;
                break;
            }
        }

        if (!skip && free_index != -1) {
            uniforms_[free_index].name = String{ name };
            uniforms_[free_index].loc = glGetUniformLocation(GLuint(prog_id_), name);
        }
    }

    // Print all uniforms
    log->Info("\tUNIFORMS");
    for (int i = 0; i < MaxUniformsCount; i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
