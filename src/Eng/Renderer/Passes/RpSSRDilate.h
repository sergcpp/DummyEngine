#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

struct RpSSRDilateData {
    RpResRef ssr_tex;
    RpResRef output_tex;
};

class RpSSRDilate : public RpExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_dilate_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpSSRDilateData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSRDilate(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const RpSSRDilateData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};