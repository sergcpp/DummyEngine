#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpBilateralBlur : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_bilateral_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    Ren::Vec2i res_ = {};
    bool vertical_ = false;

    RpResource depth_tex_;
    RpResource input_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpBilateralBlur(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, Ren::Vec2i res, bool vertical, const char depth_tex[], const char input_tex[],
               const char output_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "BILATERAL BLUR"; }
};