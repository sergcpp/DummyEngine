#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRBlur : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_ssr_blur_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource rough_tex_;
    RpResource refl_tex_;
    RpResource tile_metadata_mask_buf_;

    RpResource out_denoised_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char rough_tex_name[], const char refl_tex_name[],
               const char tile_metadata_mask_name[], const char out_denoised_img_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR BLUR"; }
};