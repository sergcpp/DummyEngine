#pragma once

#include <Ren/Pipeline.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpSSRPrefilter : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_ssr_prefilter_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    // Ren::WeakBufferRef sobol_buf_, scrambling_tile_buf_, ranking_tile_buf_;

    RpResource depth_tex_;
    RpResource norm_tex_;
    RpResource avg_refl_tex_;
    RpResource refl_tex_;
    RpResource variance_tex_;
    RpResource sample_count_tex_;
    RpResource tile_list_buf_;
    RpResource indir_args_buf_;
    uint32_t indir_args_off_ = 0;

    RpResource out_refl_tex_;
    RpResource out_variance_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[], const char norm_tex_name[],
               const char avg_refl_tex_name[], const char refl_tex_name[], Ren::WeakTex2DRef variance_tex,
               Ren::WeakTex2DRef sample_count_tex, const char tile_list_buf_name[], const char indir_args_name[],
               uint32_t indir_args_off, const char out_refl_tex_name[], Ren::WeakTex2DRef out_variance_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR PREFILTER"; }
};
