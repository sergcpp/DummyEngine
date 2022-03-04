#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSSRPrepare : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource ray_counter_buf_;
    RpResource raylen_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char ray_counter_buf_name[],
               const char raylen_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR PREPARE"; }
};
