#pragma once

#include <string>

#include <Ren/Fwd.h>

namespace Eng {
class ShaderLoader {
    static std::string ReadGLSLContent(std::string_view name, Ren::ILog *log);

  public:
#if defined(USE_GL_RENDER) || defined(USE_VK_RENDER)
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, std::string_view vs_name, std::string_view fs_name,
                                std::string_view tcs_name = {}, std::string_view tes_name = {},
                                std::string_view gs_name = {});
    Ren::ProgramRef LoadProgram(Ren::Context &ctx, std::string_view cs_name);
#endif
    Ren::ShaderRef LoadShader(Ren::Context &ctx, std::string_view name);

#if defined(USE_VK_RENDER)
    Ren::ProgramRef LoadProgram2(Ren::Context &ctx, std::string_view raygen_name, std::string_view closesthit_name,
                                 std::string_view anyhit_name, std::string_view miss_name,
                                 std::string_view intersection_name);
#endif
};
} // namespace Eng