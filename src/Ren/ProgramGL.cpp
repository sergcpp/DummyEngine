#include "ProgramGL.h"

#include "GL.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
GLuint LoadShader(GLenum shader_type, const char *source);

struct Binding {
    std::string name;
    int loc;
};
void ParseGLSLBindings(const std::string &shader_str, std::vector<Binding> &attr_bindings, std::vector<Binding> &uniform_bindings, std::vector<Binding> &uniform_block_bindings);
}

Ren::Program::Program(const char *name, const char *vs_source, const char *fs_source, eProgLoadStatus *status) {
    name_ = name;
    Init(vs_source, fs_source, status);
}

Ren::Program::Program(const char *name, const char *cs_source, eProgLoadStatus *status) {
    name_ = name;
    Init(cs_source, status);
}

Ren::Program::~Program() {
    if (prog_id_) {
        GLuint prog = (GLuint)prog_id_;
        glDeleteProgram(prog);
    }
}

Ren::Program &Ren::Program::operator=(Program &&rhs) {
    RefCounter::operator=(std::move(rhs));

    if (prog_id_) {
        GLuint prog = (GLuint)prog_id_;
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

    return *this;
}

void Ren::Program::Init(const char *vs_source, const char *fs_source, eProgLoadStatus *status) {
    InitFromGLSL({ vs_source, fs_source, nullptr }, status);
}

void Ren::Program::Init(const char *cs_source, eProgLoadStatus *status) {
    InitFromGLSL({ nullptr, nullptr, cs_source }, status);
}

void Ren::Program::InitFromGLSL(const Shaders &shaders, eProgLoadStatus *status) {
    if ((!shaders.vs_source || !shaders.fs_source) && !shaders.cs_source) {
        if (status) *status = ProgSetToDefault;
        return;
    }

    assert(!ready_);

    std::vector<Binding> attr_bindings, uniform_bindings, uniform_block_bindings;
    std::vector<Binding> *cur_bind_target = nullptr;

    GLuint program = 0;

    if (shaders.vs_source && shaders.fs_source) {
        std::string vs_source_str = shaders.vs_source, fs_source_str = shaders.fs_source;

        GLuint v_shader = LoadShader(GL_VERTEX_SHADER, vs_source_str.c_str());
        if (!v_shader) {
            fprintf(stderr, "VertexShader %s error", name_.c_str());
        }

        GLuint f_shader = LoadShader(GL_FRAGMENT_SHADER, fs_source_str.c_str());
        if (!f_shader) {
            fprintf(stderr, "FragmentShader %s error", name_.c_str());
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
                        glGetProgramInfoLog(program, buf_len, NULL, buf);
                        fprintf(stderr, "Could not link program: %s", buf);
                        free(buf);
                        throw;
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        } else {
            fprintf(stderr, "error");
            throw std::runtime_error("Program creation error!");
        }

        ParseGLSLBindings(vs_source_str, attr_bindings, uniform_bindings, uniform_block_bindings);
        ParseGLSLBindings(fs_source_str, attr_bindings, uniform_bindings, uniform_block_bindings);
    } else if (shaders.cs_source) {
        std::string cs_source_str = shaders.cs_source;

        GLuint c_shader = LoadShader(GL_COMPUTE_SHADER, cs_source_str.c_str());
        if (!c_shader) {
            fprintf(stderr, "ComputeShader %s error", name_.c_str());
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
                        glGetProgramInfoLog(program, buf_len, NULL, buf);
                        fprintf(stderr, "Could not link program: %s", buf);
                        free(buf);
                        throw;
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        } else {
            fprintf(stderr, "error");
            throw std::runtime_error("Program creation error!");
        }

        ParseGLSLBindings(cs_source_str, attr_bindings, uniform_bindings, uniform_block_bindings);
    }

    for (auto &b : attr_bindings) {
        auto &a = attributes_[b.loc];
        a.loc = glGetAttribLocation(program, b.name.c_str());
        if (a.loc != -1) {
            a.name = b.name;
        }
    }

    for (auto &b : uniform_bindings) {
        auto &u = uniforms_[b.loc];
        u.loc = glGetUniformLocation(program, b.name.c_str());
        if (u.loc != -1) {
            u.name = b.name;
        }
    }

    for (auto &b : uniform_block_bindings) {
        auto &u = uniform_blocks_[b.loc];
        u.loc = glGetUniformBlockIndex(program, b.name.c_str());
        if (u.loc != -1) {
            u.name = b.name;

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
            attributes_[free_index].name = name;
            attributes_[free_index].loc = glGetAttribLocation(program, name);
        }
    }

    printf("PROGRAM %s\n", name_.c_str());

    // Print all attributes
    printf("\tATTRIBUTES\n");
    for (int i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
        if (attributes_[i].loc == -1) {
            continue;
        }
        printf("\t\t%s : %i\n", attributes_[i].name.c_str(), attributes_[i].loc);
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
            uniforms_[free_index].name = name;
            uniforms_[free_index].loc = glGetUniformLocation(program, name);
        }
    }

    // Print all uniforms
    printf("\tUNIFORMS\n");
    for (int i = 0; i < MAX_NUM_UNIFORMS; i++) {
        if (uniforms_[i].loc == -1) {
            continue;
        }
        printf("\t\t%s : %i\n", uniforms_[i].name.c_str(), uniforms_[i].loc);
    }

    prog_id_ = (uint32_t)program;
    ready_ = true;
    if (status) *status = ProgCreatedFromData;
}

GLuint Ren::LoadShader(GLenum shader_type, const char *source) {
    GLuint shader = glCreateShader(shader_type);
    if (shader) {
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char *buf = (char *)malloc((size_t)infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    fprintf(stderr, "Could not compile shader %d: %s", int(shader_type), buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
            throw;
        }
    } else {
        fprintf(stderr, "error");
    }

    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len) {
        char *buf = (char *)malloc((size_t)info_len);
        glGetShaderInfoLog(shader, info_len, NULL, buf);
        fprintf(stderr, "%s", buf);
        free(buf);
    }

    return shader;
}

void Ren::ParseGLSLBindings(const std::string &shader_str, std::vector<Binding> &attr_bindings, std::vector<Binding> &uniform_bindings, std::vector<Binding> &uniform_block_bindings) {
    const char *delims = " \r\n\t";
    char const* p = shader_str.c_str() + shader_str.find("/*");
    char const* q = strpbrk(p + 2, delims);
    int pass = 0;

    std::vector<Binding> *cur_bind_target = nullptr;

    for (; p != NULL && q != NULL; q = strpbrk(p, delims)) {
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
            cur_bind_target = &uniform_bindings;
        } else if (item == "UNIFORM_BLOCKS") {
            cur_bind_target = &uniform_block_bindings;
        } else if (cur_bind_target) {
            p = q + 1;
            q = strpbrk(p, delims);
            if (*p != ':') {
                fprintf(stderr, "Error parsing shader!");
            }
            p = q + 1;
            q = strpbrk(p, delims);
            int loc = atoi(p);
            cur_bind_target->push_back({ item, loc });
        }

        if (!q) break;
        p = q + 1;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif