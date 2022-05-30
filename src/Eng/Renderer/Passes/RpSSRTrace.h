#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

struct RpSSRTraceData {
    RpResRef shared_data;
    RpResRef normal_tex;
    RpResRef depth_down_2x_tex;
    RpResRef output_tex;
};

class RpSSRTrace : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_prog_, blit_ssr_ms_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpSSRTraceData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSRTrace(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const RpSSRTraceData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};