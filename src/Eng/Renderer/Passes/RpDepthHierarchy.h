#pragma once

#include <Ren/Pipeline.h>

#include "../Graph/GraphBuilder.h"

struct ViewState;

class RpDepthHierarchy : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_depth_hierarchy_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource input_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex[], const char output_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DEPTH HIERARCHY"; }
};
