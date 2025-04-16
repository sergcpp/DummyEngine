#pragma once

#include <mutex>
#include <string>

#include <Ren/Pipeline.h>
#include <Ren/Program.h>
#include <Ren/RenderPass.h>
#include <Ren/Shader.h>
#include <Ren/VertexInput.h>

namespace Eng {
class ShaderLoader {
    std::mutex mtx_;
    Ren::Context &ctx_;
    Ren::VertexInputStorage vtx_inputs_;
    Ren::RenderPassStorage render_passes_;
    Ren::ShaderStorage shaders_;
    Ren::ProgramStorage programs_;
    Ren::PipelineStorage pipelines_;
    // hold references to all loaded pipelines
    std::vector<Ren::PipelineRef> persistent_pipelines_;

    static std::string ReadGLSLContent(std::string_view name, Ren::ILog *log);

  public:
    ShaderLoader(Ren::Context &ctx) : ctx_(ctx) {
        // prevent reallocation
        vtx_inputs_.reserve(32);
        render_passes_.reserve(128);
        shaders_.reserve(2048);
        programs_.reserve(1024);
        pipelines_.reserve(2048);
    }

    void LoadPipelineCache(const char *base_path);
    void WritePipelineCache(const char *base_path);

    Ren::VertexInputRef LoadVertexInput(Ren::Span<const Ren::VtxAttribDesc> attribs, const Ren::BufRef &elem_buf);

    Ren::RenderPassRef LoadRenderPass(const Ren::RenderTarget &depth_rt, Ren::Span<const Ren::RenderTarget> color_rts) {
        Ren::SmallVector<Ren::RenderTargetInfo, 4> infos;
        for (int i = 0; i < color_rts.size(); ++i) {
            infos.emplace_back(color_rts[i]);
        }
        return LoadRenderPass(Ren::RenderTargetInfo{depth_rt}, infos);
    }
    Ren::RenderPassRef LoadRenderPass(const Ren::RenderTargetInfo &depth_rt,
                                      Ren::Span<const Ren::RenderTargetInfo> color_rts);

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
    Ren::PipelineRef LoadPipeline(const Ren::ProgramRef &prog, int subgroup_size = -1);
    Ren::PipelineRef LoadPipeline(const Ren::RastState &rast_state, const Ren::ProgramRef &prog,
                                  const Ren::VertexInputRef &vtx_input, const Ren::RenderPassRef &render_pass,
                                  uint32_t subpass_index);
};
} // namespace Eng