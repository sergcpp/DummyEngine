#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class PrimDraw;

class RpTransparent : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ms_resolve_prog_;
    Ren::VertexInput draw_pass_vi_;
    Ren::RenderPass rp_transparent_;
    Ren::Framebuffer transparent_draw_fb_[Ren::MaxFramesInFlight], color_only_fb_, resolved_fb_, moments_fb_;
#if defined(USE_VK_RENDER)
    VkDescriptorSetLayout descr_set_layout_ = VK_NULL_HANDLE;
#endif

    // temp data (valid only between Setup and Execute calls)
    Ren::ApiContext *api_ctx_ = nullptr;
    const ViewState *view_state_ = nullptr;
    const Ren::Pipeline *pipelines_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const DrawList **p_list_ = nullptr;

    RpResRef vtx_buf1_;
    RpResRef vtx_buf2_;
    RpResRef ndx_buf_;
    RpResRef instances_buf_;
    RpResRef instance_indices_buf_;
    RpResRef shared_data_buf_;
    RpResRef cells_buf_;
    RpResRef items_buf_;
    RpResRef lights_buf_;
    RpResRef decals_buf_;
    RpResRef materials_buf_;
    RpResRef textures_buf_;
    RpResRef lm_tex_[4];
    RpResRef brdf_lut_;
    RpResRef noise_tex_;
    RpResRef cone_rt_lut_;
    RpResRef dummy_black_;
    RpResRef dummy_white_;

    RpResRef shad_tex_;
    RpResRef ssao_tex_;

    RpResRef color_tex_;
    RpResRef normal_tex_;
    RpResRef spec_tex_;
    RpResRef depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &color_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawTransparent(RpBuilder &builder, RpAllocTex &color_tex);

    void DrawTransparent_Simple(RpBuilder &builder, RpAllocBuf &instances_buf, RpAllocBuf &instance_indices_buf,
                                RpAllocBuf &unif_shared_data_buf, RpAllocBuf &materials_buf, RpAllocBuf &cells_buf,
                                RpAllocBuf &items_buf, RpAllocBuf &lights_buf, RpAllocBuf &decals_buf,
                                RpAllocTex &shad_tex, RpAllocTex &color_tex, RpAllocTex &ssao_tex);
    void DrawTransparent_OIT_MomentBased(RpBuilder &builder);
    void DrawTransparent_OIT_WeightedBlended(RpBuilder &builder);

#if defined(USE_VK_RENDER)
    void InitDescrSetLayout();
#endif

  public:
    RpTransparent(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}
    ~RpTransparent();

    void Setup(const DrawList **p_list, const ViewState *view_state, const RpResRef vtx_buf1, const RpResRef vtx_buf2,
               const RpResRef ndx_buf, const RpResRef materials_buf, const RpResRef textures_buf,
               const Ren::Pipeline pipelines[], const BindlessTextureData *bindless_tex, const RpResRef brdf_lut,
               const RpResRef noise_tex, const RpResRef cone_rt_lut, const RpResRef dummy_black,
               const RpResRef dummy_white, const RpResRef instances_buf, const RpResRef instance_indices_buf,
               const RpResRef shared_data_buf, const RpResRef cells_buf, const RpResRef items_buf,
               const RpResRef lights_buf, const RpResRef decals_buf, const RpResRef shad_tex, const RpResRef ssao_tex,
               const RpResRef lm_tex[4], const RpResRef color_tex, const RpResRef normal_tex, const RpResRef spec_tex,
               const RpResRef depth_tex) {
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
        shad_tex_ = shad_tex;
        ssao_tex_ = ssao_tex;
        materials_buf_ = materials_buf;
        textures_buf_ = textures_buf;

        for (int i = 0; i < 4; ++i) {
            lm_tex_[i] = lm_tex[i];
        }

        brdf_lut_ = brdf_lut;
        noise_tex_ = noise_tex;
        cone_rt_lut_ = cone_rt_lut;

        dummy_black_ = dummy_black;
        dummy_white_ = dummy_white;

        color_tex_ = color_tex;
        normal_tex_ = normal_tex;
        spec_tex_ = spec_tex;
        depth_tex_ = depth_tex;
    }

    void Execute(RpBuilder &builder) override;
};