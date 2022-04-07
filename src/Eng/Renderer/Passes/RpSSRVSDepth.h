#pragma once
#if 0
#include <Ren/Pipeline.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRVSDepth : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_ssr_vs_depth_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource depth_tex_;
    RpResource shared_data_buf_;

    RpResource out_vs_depth_img_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[],
               const char shared_data_buf_name[], const char out_vs_depth_img_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR VS DEPTH"; }
};
#endif