#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;

class RpDownDepth : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_down_depth_prog_, blit_down_depth_ms_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle depth_tex_, down_depth_2x_tex_;
    const ViewState *view_state_ = nullptr;
    int orphan_index_ = -1;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer depth_down_fb_;
#endif
  public:
    RpDownDepth(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               Ren::TexHandle depth_tex, Ren::TexHandle down_depth_2x_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DOWNSAMPLE DEPTH"; }
};