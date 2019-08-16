#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpTAA : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_taa_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer resolve_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    float reduced_average_ = 0.0f, max_exposure_ = 1.0f;

    RpResource shared_data_buf_;

    RpResource clean_tex_;
    RpResource depth_tex_;
    RpResource velocity_tex_;
    RpResource history_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex,
                  RpAllocTex &history_tex, RpAllocTex &output_tex);

  public:
    RpTAA(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakTex2DRef history_tex, float reduced_average,
               float max_exposure, const char shared_data_buf[], const char color_tex[], const char depth_tex[],
               const char velocity_tex[], const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "TAA"; }
};