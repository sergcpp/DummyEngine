#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpSSAO : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ao_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource depth_down_2x_tex_;
    RpResource rand_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSAO(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const RpResource &rand2d_dirs_4x4_tex,
               const RpResource &depth_down_2x, const RpResource &output_tex) {
        view_state_ = view_state;

        depth_down_2x_tex_ = depth_down_2x;
        rand_tex_ = rand2d_dirs_4x4_tex;

        output_tex_ = output_tex;
    }

    void Execute(RpBuilder &builder) override;
};