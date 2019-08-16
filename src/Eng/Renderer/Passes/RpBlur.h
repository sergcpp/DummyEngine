#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpBlur : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_gauss_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    bool vertical_ = false;
    const ViewState *view_state_ = nullptr;

    RpResource input_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

    
  public:
    RpBlur(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, bool vertical, Ren::WeakTex2DRef input_tex,
               const char output_tex_name[]);
    void Setup(RpBuilder &builder, const ViewState *view_state, bool vertical, const char input_tex_name[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "BLUR PASS"; }
};