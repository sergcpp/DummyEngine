#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpDownDepth : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_down_depth_prog_, blit_down_depth_ms_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer depth_down_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;

    RpResource depth_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &down_depth_2x_tex);

  public:
    RpDownDepth(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const RpResource &shared_data_buf, const RpResource &depth_tex,
               const RpResource &output_tex) {
        view_state_ = view_state;

        shared_data_buf_ = shared_data_buf;
        depth_tex_ = depth_tex;
        output_tex_ = output_tex;
    }

    void Execute(RpBuilder &builder) override;
};