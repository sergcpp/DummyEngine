#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpBlur : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_gauss_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle down_buf_4x_;
    const ViewState *view_state_ = nullptr;

    RpResource blur_temp_4x_, output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &blur_temp_4x,
                  RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer blur_fb_[2];
#endif
  public:
    RpBlur(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state,
               Ren::TexHandle down_buf_4x, const char blur_res_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "BLUR PASS"; }
};