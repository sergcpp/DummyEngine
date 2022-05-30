#pragma once

#include "../Graph/SubPass.h"

class PrimDraw;
struct ViewState;

class RpBilateralBlur : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_bilateral_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::Vec2i res_ = {};
    bool vertical_ = false;

    RpResRef depth_tex_;
    RpResRef input_tex_;

    RpResRef output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpBilateralBlur(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const Ren::Vec2i res, const bool vertical, const RpResRef depth_tex, const RpResRef input_tex,
               const RpResRef output_tex) {
        res_ = res;
        vertical_ = vertical;

        depth_tex_ = depth_tex;
        input_tex_ = input_tex;

        output_tex_ = output_tex;
    }

    void Execute(RpBuilder &builder) override;
};