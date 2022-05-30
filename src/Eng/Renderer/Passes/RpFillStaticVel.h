#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpFillStaticVel : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_static_vel_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResRef shared_data_buf_;

    RpResRef depth_tex_;
    RpResRef velocity_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex);

  public:
    RpFillStaticVel(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const RpResRef shared_data_buf, const RpResRef depth_tex,
               const RpResRef velocity_tex) {
        view_state_ = view_state;

        shared_data_buf_ = shared_data_buf;

        depth_tex_ = depth_tex;
        velocity_tex_ = velocity_tex;
    }

    void Execute(RpBuilder &builder) override;
};