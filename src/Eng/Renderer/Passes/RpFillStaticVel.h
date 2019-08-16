#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpFillStaticVel : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_static_vel_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;

    RpResource depth_tex_;
    RpResource velocity_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex);

    Ren::RenderPass render_pass_;
    Ren::Framebuffer velocity_fb_;

  public:
    RpFillStaticVel(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[], const char depth_tex[],
               const char velocity_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "FILL STATIC VEL"; }
};