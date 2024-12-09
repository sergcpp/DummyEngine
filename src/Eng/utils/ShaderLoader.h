#pragma once

#include <mutex>
#include <string>

#include <Ren/Pipeline.h>
#include <Ren/Program.h>
#include <Ren/Shader.h>

namespace Eng {
class ShaderLoader {
    Ren::Context &ctx_;
    std::mutex mtx_;
    Ren::ShaderStorage shaders_;
    Ren::ProgramStorage programs_;
    Ren::PipelineStorage pipelines_;
    // hold references to all loaded pipelines
    std::vector<Ren::PipelineRef> persistent_pipelines_;

    static std::string ReadGLSLContent(std::string_view name, Ren::ILog *log);

  public:
    ShaderLoader(Ren::Context &ctx) : ctx_(ctx) {
        // prevent reallocation
        shaders_.reserve(2048);
        programs_.reserve(1024);
        pipelines_.reserve(2048);
    }

#if defined(REN_GL_BACKEND) || defined(REN_VK_BACKEND)
    Ren::ProgramRef LoadProgram(std::string_view vs_name, std::string_view fs_name, std::string_view tcs_name = {},
                                std::string_view tes_name = {}, std::string_view gs_name = {});
    Ren::ProgramRef LoadProgram(std::string_view cs_name);
#endif
    Ren::ShaderRef LoadShader(std::string_view name);

#if defined(REN_VK_BACKEND)
    Ren::ProgramRef LoadProgram2(std::string_view raygen_name, std::string_view closesthit_name,
                                 std::string_view anyhit_name, std::string_view miss_name,
                                 std::string_view intersection_name);
#endif

    Ren::PipelineRef LoadPipeline(std::string_view cs_name, int subgroup_size = -1);
};
} // namespace Eng