#include "ProgramGL.h"

#include "../Config.h"
#include "../Log.h"
#include "GL.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Ren {
void InitBindings(const ProgramMain &prog_main, ProgramCold &prog_cold,
                  const SparseDualStorage<ShaderMain, ShaderCold> &shaders) {
    for (const ShaderROHandle sh_handle : prog_main.shaders) {
        if (!sh_handle) {
            continue;
        }

        const auto &[sh_main, sh_cold] = shaders[sh_handle];
        for (const UniformBlock &b : sh_cold.blck_bindings) {
            if (int(prog_cold.uniform_blocks.size()) < b.loc + 1) {
                prog_cold.uniform_blocks.resize(b.loc + 1);
            }
            UniformBlock &u = prog_cold.uniform_blocks[b.loc];
            u.name = b.name;

            if (!b.name.empty() && sh_cold.source == eShaderSource::GLSL) {
                u.loc = glGetUniformBlockIndex(GLuint(sh_main.id), b.name.c_str());
                if (u.loc != -1) {
                    glUniformBlockBinding(GLuint(sh_main.id), u.loc, b.loc);
                }
            }
        }
    }

    // Enumerate attributes
    GLint num;
    glGetProgramiv(GLuint(prog_main.id), GL_ACTIVE_ATTRIBUTES, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveAttrib(GLuint(prog_main.id), i, 128, &len, &len, &n, name);

        Attribute &new_attr = prog_cold.attributes.emplace_back();
        new_attr.name = String{name};
        new_attr.loc = glGetAttribLocation(GLuint(prog_main.id), name);
    }

    // Enumerate uniforms
    glGetProgramiv(GLuint(prog_main.id), GL_ACTIVE_UNIFORMS, &num);
    for (int i = 0; i < num; i++) {
        int len;
        GLenum n;
        char name[128];
        glGetActiveUniform(GLuint(prog_main.id), i, 128, &len, &len, &n, name);

        Uniform &new_uniform = prog_cold.uniforms.emplace_back();
        new_uniform.name = String{name};
        new_uniform.loc = glGetUniformLocation(GLuint(prog_main.id), name);
    }
}
} // namespace Ren

bool Ren::Program_Init(const ApiContext &api, const SparseDualStorage<ShaderMain, ShaderCold> &shaders,
                       ProgramMain &prog_main, ProgramCold &prog_cold, const ShaderROHandle vs, const ShaderROHandle fs,
                       const ShaderROHandle tcs, const ShaderROHandle tes, const ShaderROHandle gs, ILog *log) {
    assert(prog_main.id == 0);

    std::string prog_name;
    prog_name += shaders[vs].second.name;
    prog_name += "&";
    prog_name += shaders[fs].second.name;
    if (tcs && tes) {
        prog_name += "&";
        prog_name += shaders[tcs].second.name;
        prog_name += "&";
        prog_name += shaders[tes].second.name;
    }
    log->Info("Initializing program %s", prog_name.c_str());

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, GLuint(shaders[vs].first.id));
        glAttachShader(program, GLuint(shaders[fs].first.id));
        if (tcs && tes) {
            glAttachShader(program, GLuint(shaders[tcs].first.id));
            glAttachShader(program, GLuint(shaders[tes].first.id));
        }
        if (gs) {
            glAttachShader(program, GLuint(shaders[gs].first.id));
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
#ifdef REN_ENABLE_GPU_DEBUG
            glObjectLabel(GL_PROGRAM, program, -1, prog_name.c_str());
#endif
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    if (!program) {
        return false;
    }

    prog_main.id = uint32_t(program);
    // store shaders
    prog_main.shaders[int(eShaderType::Vertex)] = vs;
    prog_main.shaders[int(eShaderType::Fragment)] = fs;
    prog_main.shaders[int(eShaderType::TesselationControl)] = tcs;
    prog_main.shaders[int(eShaderType::TesselationEvaluation)] = tes;

    InitBindings(prog_main, prog_cold, shaders);

    return true;
}

bool Ren::Program_Init(const ApiContext &api, const SparseDualStorage<ShaderMain, ShaderCold> &shaders,
                       ProgramMain &prog_main, ProgramCold &prog_cold, const ShaderROHandle cs, ILog *log) {
    assert(prog_main.id == 0);

    log->Info("Initializing program %s", shaders[cs].second.name.c_str());

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, GLuint(shaders[cs].first.id));
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
#ifdef REN_ENABLE_GPU_DEBUG
            glObjectLabel(GL_PROGRAM, program, -1, shaders[cs].second.name.c_str());
#endif
        }
    } else {
        log->Error("glCreateProgram failed");
    }

    if (!program) {
        return false;
    }

    prog_main.id = uint32_t(program);
    // store shader
    prog_main.shaders[int(eShaderType::Compute)] = cs;

    InitBindings(prog_main, prog_cold, shaders);

    return true;
}

void Ren::Program_Destroy(const ApiContext &api, ProgramMain &prog_main, ProgramCold &prog_cold) {
    if (prog_main.id) {
        auto prog = GLuint(prog_main.id);
        glDeleteProgram(prog);
    }
    prog_main = {};
    prog_cold = {};
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
