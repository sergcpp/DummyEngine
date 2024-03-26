#pragma once

#include <string>

#include <Ren/Fwd.h>

namespace Eng {
class ShaderLoader {
    std::string temp_param_str_, temp_param_def_;

    static std::string ReadGLSLContent(std::string_view name, Ren::ILog *log);

  public:
    ShaderLoader();

    struct Param {
        const char *key;
        const char *val;
    };

    static int ParamsToString(const Param *params, std::string &out_str, std::string &out_def);
    static int ParamsStringToDef(const char *params, std::string &out_def);

#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
    Ren::ShaderRef LoadGLSL(Ren::Context &ctx, std::string_view name, const Param *params);

    Ren::ProgramRef LoadProgram(Ren::Context &ctx, std::string_view name, std::string_view vs_name,
                                std::string_view fs_name, std::string_view tcs_name = {},
                                std::string_view tes_name = {});
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, std::string_view name, std::string_view cs_name);
#endif
    Ren::ShaderRef LoadShader(Ren::Context &ctx, std::string_view name);

#if defined(USE_VK_RENDER)
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, std::string_view name, std::string_view raygen_name,
                                std::string_view closesthit_name, std::string_view anyhit_name,
                                std::string_view miss_name, std::string_view intersection_name);
#endif
};
} // namespace Eng