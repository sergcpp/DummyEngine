#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRTraceHQ : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_ssr_trace_hq_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;
    RpResource normal_tex_;
    RpResource depth_down_2x_tex_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[], const char normal_tex[],
               const char depth_down_2x[], const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR TRACE HQ"; }
};