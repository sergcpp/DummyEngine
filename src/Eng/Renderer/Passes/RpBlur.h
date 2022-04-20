#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

struct RpBlurData {
    RpResRef input_tex;
    RpResRef output_tex;
};

class RpBlur : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_gauss_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    bool vertical_ = false;
    const ViewState *view_state_ = nullptr;
    const RpBlurData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

    
  public:
    RpBlur(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, bool vertical, const RpBlurData *pass_data) {
        vertical_ = vertical;
        view_state_ = view_state;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};