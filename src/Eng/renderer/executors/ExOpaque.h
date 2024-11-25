#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

#include <Ren/VertexInput.h>

namespace Eng {
class ExOpaque final : public FgExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::VertexInput draw_pass_vi_;
    Ren::RenderPass rp_opaque_;
    Ren::Framebuffer opaque_draw_fb_[Ren::MaxFramesInFlight][2];
    int fb_to_use_ = 0;
#if defined(REN_VK_BACKEND)
    VkDescriptorSetLayout descr_set_layout_ = VK_NULL_HANDLE;
#endif

    // temp data (valid only between Setup and Execute calls)
    Ren::ApiContext *api_ctx_ = nullptr;
    const ViewState *view_state_ = nullptr;
    const Ren::Pipeline *pipelines_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const DrawList **p_list_ = nullptr;

    FgResRef vtx_buf1_;
    FgResRef vtx_buf2_;
    FgResRef ndx_buf_;
    FgResRef instances_buf_;
    FgResRef instance_indices_buf_;
    FgResRef shared_data_buf_;
    FgResRef materials_buf_;
    FgResRef textures_buf_;
    FgResRef cells_buf_;
    FgResRef items_buf_;
    FgResRef lights_buf_;
    FgResRef decals_buf_;
    FgResRef shad_tex_;
    FgResRef lm_tex_[4];
    FgResRef ssao_tex_;
    FgResRef brdf_lut_;
    FgResRef noise_tex_;
    FgResRef cone_rt_lut_;
    FgResRef dummy_black_;

    FgResRef color_tex_;
    FgResRef normal_tex_;
    FgResRef spec_tex_;
    FgResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocBuf &vtx_buf1, FgAllocBuf &vtx_buf2,
                  FgAllocBuf &ndx_buf, FgAllocTex &color_tex, FgAllocTex &normal_tex, FgAllocTex &spec_tex,
                  FgAllocTex &depth_tex);
    void DrawOpaque(FgBuilder &builder);

#if defined(REN_VK_BACKEND)
    void InitDescrSetLayout();
#endif
  public:
    ~ExOpaque() final;

    void Setup(const DrawList **p_list, const ViewState *view_state, const FgResRef vtx_buf1, const FgResRef vtx_buf2,
               const FgResRef ndx_buf, const FgResRef materials_buf, const FgResRef textures_buf,
               const Ren::Pipeline pipelines[], const BindlessTextureData *bindless_tex, const FgResRef brdf_lut,
               const FgResRef noise_tex, const FgResRef cone_rt_lut, const FgResRef dummy_black,
               const FgResRef instances_buf, const FgResRef instance_indices_buf, const FgResRef shared_data_buf,
               const FgResRef cells_buf, const FgResRef items_buf, const FgResRef lights_buf, const FgResRef decals_buf,
               const FgResRef shadowmap_tex, const FgResRef ssao_tex, const FgResRef lm_tex[], const FgResRef out_color,
               const FgResRef out_normals, const FgResRef out_spec, const FgResRef out_depth) {
        view_state_ = view_state;
        pipelines_ = pipelines;
        bindless_tex_ = bindless_tex;

        p_list_ = p_list;

        vtx_buf1_ = vtx_buf1;
        vtx_buf2_ = vtx_buf2;
        ndx_buf_ = ndx_buf;
        instances_buf_ = instances_buf;
        instance_indices_buf_ = instance_indices_buf;
        shared_data_buf_ = shared_data_buf;
        cells_buf_ = cells_buf;
        items_buf_ = items_buf;
        lights_buf_ = lights_buf;
        decals_buf_ = decals_buf;
        shad_tex_ = shadowmap_tex;

        materials_buf_ = materials_buf;
        ssao_tex_ = ssao_tex;

        for (int i = 0; i < 4; ++i) {
            lm_tex_[i] = lm_tex[i];
        }

        brdf_lut_ = brdf_lut;
        noise_tex_ = noise_tex;
        cone_rt_lut_ = cone_rt_lut;

        dummy_black_ = dummy_black;

        textures_buf_ = textures_buf;

        color_tex_ = out_color;
        normal_tex_ = out_normals;
        spec_tex_ = out_spec;
        depth_tex_ = out_depth;
    }
    void Execute(FgBuilder &builder) override;
};
} // namespace Eng