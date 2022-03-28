#pragma once

#include "../Graph/RenderPass.h"

class PrimDraw;
struct ViewState;

class RpBilateralBlur : public RenderPassExecutor {
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

    void Setup(const Ren::Vec2i res, const bool vertical, const RpResource &depth_tex, const RpResource &input_tex,
               const RpResource &output_tex) {
        res_ = res;
        vertical_ = vertical;

        depth_tex_ = depth_tex;
        input_tex_ = input_tex;

        output_tex_ = output_tex;
    }

    void Execute(RpBuilder &builder) override;
};