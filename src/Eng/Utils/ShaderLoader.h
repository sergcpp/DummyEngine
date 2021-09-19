#pragma once

#include <string>

#include <Ren/Fwd.h>

class ShaderLoader {
    std::string temp_param_str_, temp_param_def_;

    static std::string ReadGLSLContent(const char *name, int name_len, const char *prelude, Ren::ILog *log);

  public:
    ShaderLoader();

    struct Param {
        const char *key;
        const char *val;
    };

    static int ParamsToString(const Param *params, std::string &out_str, std::string &out_def);
    static int ParamsStringToDef(const char *params, std::string &out_def);

#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
    Ren::ShaderRef LoadGLSL(Ren::Context &ctx, const char *name, const Param *params);

    Ren::ProgramRef LoadProgram(Ren::Context &ctx, const char *name, const char *vs_name, const char *fs_name,
                                const char *tcs_name = nullptr, const char *tes_name = nullptr);
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, const char *name, const char *cs_name);
#endif
    Ren::ShaderRef LoadShader(Ren::Context &ctx, const char *name);

#if defined(USE_VK_RENDER)
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, const char *name, const char *raygen_name,
                                const char *closesthit_name, const char *anyhit_name, const char *miss_name,
                                const char *intersection_name);
#endif
};