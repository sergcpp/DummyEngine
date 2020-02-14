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

struct Binding {
    String  name;
    int     loc;
};
void ParseGLSLBindings(const char *shader_str, Binding *attr_bindings, int &attr_bindings_count,
    Binding *uniform_bindings, int &uniform_bindings_count,
    Binding *uniform_block_bindings, int &uniform_block_bindings_count, ILog *log);
}

Ren::Program::Program(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *status, ILog *log) {
    name_ = String{ name };
    Init(vs_source, fs_source, status, log);
}

Ren::Program::Program(const char *name, const char *cs_source, eProgLoadStatus *status, ILog *log) {
    name_ = String{ name };
    Init(cs_source, status, log);
}

#ifndef __ANDROID__
Ren::Program::Program(const char *name, const uint8_t *vs_data, const int vs_data_size,
        const uint8_t *fs_data, const int fs_data_size, eProgLoadStatus *status, ILog *log) {
    name_ = String{ name };
    Init(vs_data, vs_data_size, fs_data, fs_data_size, status, log);
}

Ren::Program::Program(const char *name, const uint8_t *cs_data, const int cs_data_size, eProgLoadStatus *status, ILog *log) {
    name_ = String{ name };
    Init(cs_data, cs_data_size, status, log);
}
#endif

Ren::Program::~Program() {
    if (prog_id_) {
        auto prog = (GLuint)prog_id_;
        glDeleteProgram(prog);
    }
}

Ren::Program &Ren::Program::operator=(Program &&rhs) noexcept {
    if (prog_id_) {
        auto prog = (GLuint)prog_id_;
        glDeleteProgram(prog);
    }

    prog_id_ = rhs.prog_id_;
    rhs.prog_id_ = 0;
    attributes_ = std::move(rhs.attributes_);
    uniforms_ = std::move(rhs.uniforms_);
    uniform_blocks_ = std::move(rhs.uniform_blocks_);
    ready_ = rhs.ready_;
    rhs.ready_ = false;
    name_ = std::move(rhs.name_);

    RefCounter::operator=(std::move(rhs));

    return *this;
}

void Ren::Program::Init(const char *vs_source, const char *fs_source, eProgLoadStatus *status, ILog *log) {
    InitFromGLSL({ vs_source, fs_source, nullptr }, status, log);
}

void Ren::Program::Init(const char *cs_source, eProgLoadStatus *status, ILog *log) {
    InitFromGLSL({ nullptr, nullptr, cs_source }, status, log);
}

#ifndef __ANDROID__
void Ren::Program::Init(const uint8_t *vs_data, const int vs_data_size,
                        const uint8_t *fs_data, const int fs_data_size, eProgLoadStatus *status, ILog *log) {
    InitFromSPIRV({ vs_data, vs_data_size, fs_data, fs_data_size, nullptr, 0 }, status, log);
}

void Ren::Program::Init(const uint8_t *cs_data, const int cs_data_size, eProgLoadStatus *status, ILog *log) {
    InitFromSPIRV({ nullptr, 0, nullptr, 0, cs_data, cs_data_size }, status, log);
}
#endif

void Ren::Program::InitFromGLSL(const ShadersSrc &shaders, eProgLoadStatus *status, ILog *log) {
    if ((!shaders.vs_source || !shaders.fs_source) && !shaders.cs_source) {
        if (status) *status = ProgSetToDefault;
        return;
    }

    assert(!ready_);

    Binding attr_bindings[MAX_NUM_ATTRIBUTES], uniform_bindings[MAX_NUM_UNIFORMS], uniform_block_bindings[MAX_NUM_UNIFORM_BLOCKS];
    int attr_bindings_count = 0, uniform_bindings_count = 0, uniform_block_bindings_count = 0;

    GLuint program = 0;

    if (shaders.vs_source && shaders.fs_source) {
        const GLuint v_shader = LoadShader(GL_VERTEX_SHADER, shaders.vs_source, log);
        if (!v_shader) {
            log->Error("VertexShader %s error", name_.c_str());
        }

        const GLuint f_shader = LoadShader(GL_FRAGMENT_SHADER, shaders.fs_source, log);
        if (!f_shader) {
            log->Error("FragmentShader %s error", name_.c_str());
        }

        program = glCreateProgram();
        if (program) {
            glAttachShader(program, v_shader);
            glAttachShader(program, f_shader);
            glLinkProgram(program);
            GLint link_status = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &link_status);
            if (link_status != GL_TRUE) {
                GLint buf_len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
                if (buf_len) {
                    char *buf = (char *)malloc((size_t)buf_len);
                    if (buf) {
                        glGetProgramInfoLog(program, buf_len, nullptr, buf);
                        log->Error("Could not link program: %s", buf);
                        free(buf);
                        throw;
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        } else {
            log->Error("glCreateProgram failed");
            throw std::runtime_error("Program creation error!");
        }

        ParseGLSLBindings(
            shaders.vs_source, attr_bindings, attr_bindings_count, uniform_bindings, uniform_bindings_count,
            uniform_block_bindings, uniform_block_bindings_count, log);
        ParseGLSLBindings(
            shaders.fs_source, attr_bindings, attr_bindings_count, uniform_bindings, uniform_bindings_count,
            uniform_block_bindings, uniform_block_bindings_count, log);
    } else if (shaders.cs_source) {
        const GLuint c_shader = LoadShader(GL_COMPUTE_SHADER, shaders.cs_source, log);
        if (!c_shader) {
            log->Error("ComputeShader %s error", name_.c_str());
        }

        program = glCreateProgram();
        if (program) {
            glAttachShader(program, c_shader);
            glLinkProgram(program);
            GLint link_status = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &link_status);
            if (link_status != GL_TRUE) {
                GLint buf_len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
                if (buf_len) {
                    char *buf = (char *)malloc((size_t)buf_len);
                    if (buf) {
                        glGetProgramInfoLog(program, buf_len, nullptr, buf);
                        log->Error( "Could not link program: %s", buf);
                        free(buf);
                        throw;
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        } else {
            log->Error("glCreateProgram failed");
            throw std::runtime_error("Program creation error!");
        }

        ParseGLSLBindings(
            shaders.cs_source, attr_bindings, attr_bindings_count, uniform_bindings, uniform_bindings_count,
            uniform_block_bindings, uniform_block_bindings_count, log);
    }

    for (int i = 0; i < attr_bindings_count; i++) {
        Binding &b = attr_bindings[i];
        Attribute &a = attributes_[b.loc];
        a.loc = glGetAttribLocation(program, b.name.c_str());
        if (a.loc != -1) {
            a.name = std::move(b.name);
        }
    }

    for (int i = 0; i < uniform_bindings_count; i++) {
        Binding &b = uniform_bindings[i];
        Attribute &u = uniforms_[b.loc];
        u.loc = glGetUniformLocation(program, b.name.c_str());
        if (u.loc != -1) {
            u.name = std::move(b.name);
        }
    }

    for (int i = 0; i < uniform_block_bindings_count; i++) {
        Binding &b = uniform_block_bindings[i];
        Attribute &u = uniform_blocks_[b.loc];
        u.loc = glGetUniformBlockIndex(program, b.name.c_str());
        if (u.loc != -1) {
            u.name = std::move(b.name);
            glUniformBlockBinding(program, u.loc, b.loc);
        }
    }

    // Enumerate rest of attributes
    int num;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveAttrib(program, i, 128, &len, &len, &n, name);

        int skip = 0, free_index = -1;
        for (int j = 0; j < MAX_NUM_ATTRIBUTES; j++) {
            if (free_index == -1 && attributes_[j].loc == -1) {
                free_index = j;
            }
            if (attributes_[j].loc != -1 && attributes_[j].name == name) {
                skip = 1;
                break;
            }
        }

        if (!skip && free_index != -1) {
            attributes_[free_index].name = String{ name };
            attributes_[free_index].loc = glGetAttribLocation(program, name);
        }
    }

    log->Info("PROGRAM %s", name_.c_str());

    // Print all attributes
    log->Info("\tATTRIBUTES");
    for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", attributes_[i].name.c_str(), attributes_[i].loc);
    }

    // Enumerate rest of uniforms
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveUniform(program, i, 128, &len, &len, &n, name);

        int skip = 0, free_index = -1;
        for (int j = 0; j < MAX_NUM_UNIFORMS; j++) {
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
            uniforms_[free_index].loc = glGetUniformLocation(program, name);
        }
    }

    // Print all uniforms
    log->Info("\tUNIFORMS");
    for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }

    prog_id_ = (uint32_t)program;
    ready_ = true;
    if (status) *status = ProgCreatedFromData;
}

#ifndef __ANDROID__
void Ren::Program::InitFromSPIRV(const ShadersBin &shaders, eProgLoadStatus *status, ILog *log) {
    if ((!shaders.vs_data || !shaders.fs_data) && !shaders.cs_data) {
        if (status) *status = ProgSetToDefault;
        return;
    }

    assert(!ready_);

    GLuint program;
    if (shaders.vs_data && shaders.fs_data) {
        GLuint v_shader = LoadShader(GL_VERTEX_SHADER, shaders.vs_data, shaders.vs_data_size, log);
        if (!v_shader) {
            log->Error("VertexShader %s error", name_.c_str());
        }

        GLuint f_shader = LoadShader(GL_FRAGMENT_SHADER, shaders.fs_data, shaders.fs_data_size, log);
        if (!f_shader) {
            log->Error("FragmentShader %s error", name_.c_str());
        }

        program = glCreateProgram();
        if (program) {
            glAttachShader(program, v_shader);
            glAttachShader(program, f_shader);
            glLinkProgram(program);
            GLint link_status = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &link_status);
            if (link_status != GL_TRUE) {
                GLint buf_len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_len);
                if (buf_len) {
                    char *buf = (char *)malloc((size_t)buf_len);
                    if (buf) {
                        glGetProgramInfoLog(program, buf_len, nullptr, buf);
                        log->Error("Could not link program: %s", buf);
                        free(buf);
                        throw std::runtime_error("Program linking error!");
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        } else {
            log->Error("glCreateProgram failed");
            throw std::runtime_error("Program creation error!");
        }
    } else if (shaders.cs_data) {
        // TODO: !!!
    }

    // Enumerate attributes
    int num;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &num);
    for (int i = 0; i < num; i++) {
        attributes_[i].name = String{}; // TODO
        attributes_[i].loc = i;
    }

    // Enumerate uniforms
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num);
    for (int i = 0; i < num; i++) {
        uniforms_[i].name = String{}; // TODO
        uniforms_[i].loc = i;
    }

    log->Info("PROGRAM %s", name_.c_str());

    // Print all attributes
    log->Info("\tATTRIBUTES");
    for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", attributes_[i].name.c_str(), attributes_[i].loc);
    }

    // Print all uniforms
    log->Info("\tUNIFORMS");
    for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        log->Info("\t\t%s : %i", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }

    prog_id_ = (uint32_t)program;
    ready_ = true;
    if (status) *status = ProgCreatedFromData;
}
#endif

GLuint Ren::LoadShader(GLenum shader_type, const char *source, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *)malloc((size_t)infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, nullptr, buf);
                    log->Error("Could not compile shader %d: %s", int(shader_type), buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
            throw std::runtime_error("Error compiling shader!");
        }
    } else {
        log->Error("glCreateShader failed");
        throw std::runtime_error("Error creating shader!");
    }

    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len) {
        char *buf = (char *)malloc((size_t)info_len);
        glGetShaderInfoLog(shader, info_len, NULL, buf);
        log->Info("%s", buf);
        free(buf);
    }

    return shader;
}

#ifndef __ANDROID__
GLuint Ren::LoadShader(GLenum shader_type, const uint8_t *data, const int data_size, ILog *log) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, data, static_cast<GLsizei>(data_size));
        glSpecializeShader(shader, "main", 0, nullptr, nullptr);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *)malloc((size_t)infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, nullptr, buf);
                    log->Error("Could not compile shader %d: %s", int(shader_type), buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
            throw std::runtime_error("Error compiling shader!");
        }
    } else {
        log->Error("glCreateShader failed");
        throw std::runtime_error("Error creating shader!");
    }

    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len) {
        char *buf = (char *)malloc((size_t)info_len);
        glGetShaderInfoLog(shader, info_len, nullptr, buf);
        log->Error("%s", buf);
        free(buf);
    }

    return shader;
}
#endif

void Ren::ParseGLSLBindings(const char *shader_str, Binding *attr_bindings, int &attr_bindings_count,
                            Binding *uniform_bindings, int &uniform_bindings_count,
                            Binding *uniform_block_bindings, int &uniform_block_bindings_count, ILog *log) {
    const char *delims = " \r\n\t";
    const char *p = strstr(shader_str, "/*");
    const char *q = p ? strpbrk(p + 2, delims) : nullptr;
    int pass = 0;

    Binding *cur_bind_target = nullptr;
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
            cur_bind_target = attr_bindings;
            cur_bind_count = &attr_bindings_count;
        } else if (item == "UNIFORMS") {
            cur_bind_target = uniform_bindings;
            cur_bind_count = &uniform_bindings_count;
        } else if (item == "UNIFORM_BLOCKS") {
            cur_bind_target = uniform_block_bindings;
            cur_bind_count = &uniform_block_bindings_count;
        } else if (cur_bind_target) {
            p = q + 1;
            q = strpbrk(p, delims);
            if (*p != ':') {
                log->Error("Error parsing shader!");
            }
            p = q + 1;
            q = strpbrk(p, delims);
            int loc = atoi(p);

            cur_bind_target[*cur_bind_count].name = item;
            cur_bind_target[*cur_bind_count].loc = loc;
            (*cur_bind_count)++;
        }

        if (!q) break;
        p = q + 1;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif