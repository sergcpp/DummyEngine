#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/VertexInput.h>

class PrimDraw;

class RpTransparent : public RenderPassBase {
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

    const EnvironmentWeak *env_ = nullptr;
    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const Ren::ProbeStorage *probe_storage_ = nullptr;

    uint64_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<CustomDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;
    int alpha_blend_start_index_ = -1;

    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;
    RpResource instances_buf_;
    RpResource instance_indices_buf_;
    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;
    RpResource materials_buf_;
    RpResource textures_buf_;
    RpResource lm_tex_[4];
    RpResource brdf_lut_;
    RpResource noise_tex_;
    RpResource cone_rt_lut_;
    RpResource dummy_black_;
    RpResource dummy_white_;

    RpResource shad_tex_;
    RpResource color_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;
    RpResource ssao_tex_;
    RpResource transparent_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &color_tex, RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex,
                  RpAllocTex &transparent_tex);
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

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, const Ren::BufferRef &vtx_buf1,
               const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf, const Ren::BufferRef &materials_buf,
               const Ren::Pipeline pipelines[], const BindlessTextureData *bindless_tex, const Ren::Tex2DRef &brdf_lut,
               const Ren::Tex2DRef &noise_tex, const Ren::Tex2DRef &cone_rt_lut, const Ren::Tex2DRef &dummy_black,
               const Ren::Tex2DRef &dummy_white, const char instances_buf[], const char instance_indices_buf[],
               const char shared_data_buf[], const char cells_buf[], const char items_buf[], const char lights_buf[],
               const char decals_buf[], const char shad_tex[], const char ssao_tex[], const char color_tex[],
               const char normal_tex[], const char spec_tex[], const char depth_tex[],
               const char transparent_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "TRANSPARENT"; }
};