#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRTrace : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_prog_, blit_ssr_ms_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    Ren::WeakTex2DRef down_buf_4x_tex_;
    Ren::Tex2DRef brdf_lut_;
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_down_2x_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSRTrace(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state,
               Ren::WeakTex2DRef down_buf_4x_tex, Ren::Tex2DRef brdf_lut, const char shared_data_buf[],
               const char normal_tex[], const char spec_tex[], const char depth_down_2x[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR TRACE"; }
};