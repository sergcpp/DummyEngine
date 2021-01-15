#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;

class RpReflections : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    int orphan_index_ = -1;
    Ren::ProgramRef blit_ssr_prog_, blit_ssr_ms_prog_, blit_ssr_dilate_prog_,
        blit_ssr_compose_prog_, blit_ssr_compose_ms_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle depth_tex_, norm_tex_, spec_tex_, down_depth_2x_tex_, down_buf_4x_tex_;
    Ren::TexHandle ssr_buf1_tex_, ssr_buf2_tex_, output_tex_;
    Ren::Tex2DRef brdf_lut_;
    const ViewState *view_state_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer ssr_buf1_fb_, ssr_buf2_fb_, output_fb_;
#endif
  public:
    RpReflections(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               const ProbeStorage *probe_storage, Ren::TexHandle depth_tex,
               Ren::TexHandle norm_tex, Ren::TexHandle spec_tex,
               Ren::TexHandle down_depth_2x_tex, Ren::TexHandle down_buf_4x_tex,
               Ren::TexHandle ssr_buf1_tex, Ren::TexHandle ssr_buf2_tex,
               Ren::Tex2DRef brdf_lut, Ren::TexHandle output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "REFLECTIONS PASS"; }
};