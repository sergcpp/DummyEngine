#pragma once

#include <Ren/Pipeline.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRResolveSpatial : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_ssr_resolve_spatial_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    // Ren::WeakBufferRef sobol_buf_, scrambling_tile_buf_, ranking_tile_buf_;

    RpResource depth_tex_;
    RpResource norm_tex_;
    RpResource rough_tex_;
    RpResource relf_tex_;
    RpResource tile_metadata_mask_buf_;

    RpResource out_denoised_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[], const char norm_tex_name[],
               const char rough_tex_name[], const char refl_tex_name[], const char tile_metadata_mask_name[],
               const char denoised_img_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR RESOLVE SPATIAL"; }
};
