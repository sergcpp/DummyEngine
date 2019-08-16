#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpUpscale : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_upscale_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    Ren::Vec4i res_;
    Ren::Vec4f clip_info_;

    RpResource depth_tex_;
    RpResource depth_down_2x_tex_;
    RpResource input_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpUpscale(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const Ren::Vec4i &res, const Ren::Vec4f &clip_info, const char depth_down_2x[],
               const char depth_tex[], const char input_tex[], const char output_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "UPSCALE"; }
};
