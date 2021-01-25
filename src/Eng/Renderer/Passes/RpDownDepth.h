#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpDownDepth : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_down_depth_prog_, blit_down_depth_ms_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    int orphan_index_ = -1;

    RpResource shared_data_buf_;

    RpResource depth_tex_;
    RpResource depth_down_2x_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &down_depth_2x_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer depth_down_fb_;
#endif
  public:
    RpDownDepth(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               const char shared_data_buf[], const char depth_tex[],
               const char output_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DOWNSAMPLE DEPTH"; }
};