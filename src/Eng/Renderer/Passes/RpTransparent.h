#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpTransparent : public Graph::RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;
    int orphan_index_ = 0;

    // lazily initialized data
    Ren::ProgramRef blit_ms_resolve_prog_;
    Ren::Tex2DRef dummy_black_, dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle color_tex_, normal_tex_, spec_tex_, depth_tex_, transparent_tex_;
    Ren::TexHandle moments_b0_, moments_z_and_z2_, moments_z3_and_z4_;
    const ViewState *view_state_ = nullptr;
    Ren::Tex1DRef instances_tbo_, lights_tbo_, decals_tbo_, cells_tbo_, items_tbo_;
    Ren::Tex2DRef brdf_lut_, noise_tex_, cone_rt_lut_;

    Ren::TexHandle shadow_tex_, ssao_tex_;
    const Environment *env_ = nullptr;
    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    uint32_t render_flags_ = 0;
    DynArrayConstRef<MainDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;
    const int *alpha_blend_start_index_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
    void DrawTransparent(Graph::RpBuilder &builder);

    void DrawTransparent_Simple(Graph::RpBuilder &builder,
                                Graph::AllocatedBuffer &unif_shared_data_buf);
    void DrawTransparent_OIT_MomentBased(Graph::RpBuilder &builder);
    void DrawTransparent_OIT_WeightedBlended(Graph::RpBuilder &builder);

#if defined(USE_GL_RENDER)
    Ren::Vao draw_pass_vao_;

    Ren::Framebuffer transparent_draw_fb_, color_only_fb_, resolved_fb_, moments_fb_;
#endif
  public:
    RpTransparent(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(Graph::RpBuilder &builder, const DrawList &list,
               const int *alpha_blend_start_index, const ViewState *view_state,
               int orphan_index, Ren::TexHandle color_tex, Ren::TexHandle normal_tex,
               Ren::TexHandle spec_tex, Ren::TexHandle depth_tex,
               Ren::TexHandle transparent_tex, Ren::TexHandle moments_b0,
               Ren::TexHandle moments_z_and_z2, Ren::TexHandle moments_z3_and_z4,
               Graph::ResourceHandle in_instances_buf, Ren::Tex1DRef instances_tbo,
               Graph::ResourceHandle in_shared_data_buf, Ren::TexHandle shadow_tex,
               Ren::TexHandle ssao_tex, Ren::Tex1DRef lights_tbo,
               Ren::Tex1DRef decals_tbo, Ren::Tex1DRef cells_tbo,
               Ren::Tex1DRef items_tbo, Ren::Tex2DRef brdf_lut,
               Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut);
    void Execute(Graph::RpBuilder &builder) override;

    const char *name() const override { return "TRANSPARENT PASS"; }
};