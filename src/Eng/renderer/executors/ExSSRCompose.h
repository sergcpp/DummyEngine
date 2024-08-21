#pragma once

#include "../framegraph/FgNode.h"

namespace Eng {
class PrimDraw;
struct ViewState;
class ExSSRCompose : public FgExecutor {
  public:
    ExSSRCompose(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    struct Args {
        FgResRef shared_data;
        FgResRef cells_buf;
        FgResRef items_buf;

        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef spec_tex;
        FgResRef depth_down_2x_tex;
        FgResRef down_buf_4x_tex;
        FgResRef ssr_tex;
        FgResRef brdf_lut;

        FgResRef output_tex;
    };

    void Setup(FgBuilder &builder, const ViewState *view_state, const Ren::ProbeStorage *probe_storage,
               const Args *args) {
        view_state_ = view_state;
        probe_storage_ = probe_storage;
        args_ = args;
    }
    void Execute(FgBuilder &builder) override;

  private:
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_compose_prog_, blit_ssr_compose_ms_prog_;
    Ren::ProgramRef blit_ssr_compose_hq_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Ren::ProbeStorage *probe_storage_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, FgAllocTex &output_tex);
};
} // namespace Eng