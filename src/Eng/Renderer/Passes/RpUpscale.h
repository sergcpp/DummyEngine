#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpUpscale : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_upscale_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    Ren::Vec4i res_;
    Ren::Vec4f clip_info_;

    RpResRef depth_tex_;
    RpResRef depth_down_2x_tex_;
    RpResRef input_tex_;

    RpResRef output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpUpscale(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const Ren::Vec4i &res, const Ren::Vec4f &clip_info, const RpResRef depth_down_2x,
               const RpResRef depth_tex, const RpResRef input_tex, const RpResRef output_tex) {
        res_ = res;
        clip_info_ = clip_info;

        depth_down_2x_tex_ = depth_down_2x;
        depth_tex_ = depth_tex;
        input_tex_ = input_tex;

        output_tex_ = output_tex;
    }

    void Execute(RpBuilder &builder) override;
};
