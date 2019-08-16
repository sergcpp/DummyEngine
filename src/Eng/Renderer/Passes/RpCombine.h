#pragma once

#include <Ren/RenderPass.h>
#include <Ren/Texture.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpCombine : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;
    bool tonemap_ = false;
    float gamma_ = 1.0f, exposure_ = 1.0f, fade_ = 0.0f;

    // lazily initialized data
    Ren::ProgramRef blit_combine_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource color_tex_;
    RpResource blur_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex);

    Ren::RenderPass render_pass_[Ren::MaxFramesInFlight];
    Ren::Framebuffer output_fb_[Ren::MaxFramesInFlight];

  public:
    RpCombine(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::Tex2DRef dummy_black, float gamma, float exposure,
               float fade, bool tonemap, const char color_tex_name[], const char blur_tex_name[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "COMBINE PASS"; }
};