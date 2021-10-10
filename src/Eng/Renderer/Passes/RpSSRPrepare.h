#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSSRPrepare : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource temp_variance_mask_buf_;
    RpResource ray_counter_buf_;
    RpResource denoised_refl_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char temp_variance_mask_name[],
               const char ray_counter_name[], const char denoised_refl_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR PREPARE"; }
};
