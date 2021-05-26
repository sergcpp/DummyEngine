#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class PrimDraw;

class RpTransparent : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;
    int orphan_index_ = 0;

    // lazily initialized data
    Ren::ProgramRef blit_ms_resolve_prog_;
    Ren::Tex2DRef dummy_black_, dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    Ren::Tex2DRef brdf_lut_, noise_tex_, cone_rt_lut_;
    const PersistentBuffers *bufs_ = nullptr;

    const EnvironmentWeak *env_ = nullptr;
    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    uint32_t render_flags_ = 0;
    const Ren::MaterialStorage *materials_ = nullptr;
    DynArrayConstRef<MainDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;
    const int *alpha_blend_start_index_ = nullptr;

    RpResource instances_buf_;
    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;

    RpResource shadowmap_tex_;
    RpResource color_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;
    RpResource ssao_tex_;
    RpResource transparent_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex,
                  RpAllocTex &normal_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex,
                  RpAllocTex &transparent_tex);
    void DrawTransparent(RpBuilder &builder, RpAllocTex &color_tex);

    void DrawTransparent_Simple(RpBuilder &builder, RpAllocBuf &instances_buf,
                                RpAllocBuf &unif_shared_data_buf, RpAllocBuf &cells_buf,
                                RpAllocBuf &items_buf, RpAllocBuf &lights_buf,
                                RpAllocBuf &decals_buf, RpAllocTex &shadowmap_tex,
                                RpAllocTex &color_tex, RpAllocTex &ssao_tex);
    void DrawTransparent_OIT_MomentBased(RpBuilder &builder);
    void DrawTransparent_OIT_WeightedBlended(RpBuilder &builder);

#if defined(USE_GL_RENDER)
    Ren::Vao draw_pass_vao_;

    Ren::Framebuffer transparent_draw_fb_, color_only_fb_, resolved_fb_, moments_fb_;
#endif
  public:
    RpTransparent(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const DrawList &list,
               const int *alpha_blend_start_index, const ViewState *view_state,
               const PersistentBuffers *bufs, int orphan_index, Ren::Tex2DRef brdf_lut,
               Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut,
               const char instances_buf[], const char shared_data_buf[],
               const char cells_buf[], const char items_buf[], const char lights_buf[],
               const char decals_buf[], const char shadowmap_tex[], const char ssao_tex[],
               const char color_tex[], const char normal_tex[], const char spec_tex[],
               const char depth_tex[], const char transparent_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "TRANSPARENT PASS"; }
};