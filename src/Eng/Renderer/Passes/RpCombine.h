#pragma once

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
    Ren::TexHandle color_tex_, blur_tex_, output_tex_;
    const ViewState *view_state_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer output_fb_;
#endif
  public:
    RpCombine(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, float gamma,
               float exposure, float fade, bool tonemap, Ren::TexHandle color_tex,
               Ren::TexHandle blur_tex, Ren::TexHandle output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "COMBINE PASS"; }
};