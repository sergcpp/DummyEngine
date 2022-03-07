#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSSRWriteIndirectRTDispatch : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;
    Ren::Pipeline pi_write_indirect_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource ray_counter_buf_;
    RpResource indir_disp_buf_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char ray_counter_name[],
               const char indir_disp_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "RT DISPATCH ARGS"; }
};
