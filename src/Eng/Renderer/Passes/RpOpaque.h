#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpOpaque : public RenderPassBase {
    bool initialized = false;
    int orphan_index_ = 0;

    // lazily initialized data
    Ren::Tex2DRef dummy_black_, dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle color_tex_, normal_tex_, spec_tex_, depth_tex_;
    const ViewState *view_state_ = nullptr;
    Ren::Tex2DRef brdf_lut_, noise_tex_, cone_rt_lut_;

    Ren::TexHandle shadow_tex_, ssao_tex_;
    const Environment *env_ = nullptr;
    const Ren::TextureAtlas *decals_atlas_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    uint32_t render_flags_ = 0;
    DynArrayConstRef<MainDrawBatch> main_batches_;
    DynArrayConstRef<uint32_t> main_batch_indices_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
    void DrawOpaque(RpBuilder &builder);

#if defined(USE_GL_RENDER)
    Ren::Vao draw_pass_vao_;

    Ren::Framebuffer opaque_draw_fb_;
#endif
  public:
    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               int orphan_index, Ren::TexHandle color_tex, Ren::TexHandle normal_tex,
               Ren::TexHandle spec_tex, Ren::TexHandle depth_tex,
               Ren::TexHandle shadow_tex, Ren::TexHandle ssao_tex, Ren::Tex2DRef brdf_lut,
               Ren::Tex2DRef noise_tex, Ren::Tex2DRef cone_rt_lut);
    void Execute(RpBuilder &builder) override;

    // TODO: remove this
    int alpha_blend_start_index_ = -1;

    const char *name() const override { return "OPAQUE PASS"; }
};