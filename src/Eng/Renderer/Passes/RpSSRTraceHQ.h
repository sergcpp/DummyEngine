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
    // Ren::WeakBufferRef sobol_buf_, scrambling_tile_buf_, ranking_tile_buf_;

    RpResource sobol_buf_;
    RpResource scrambling_tile_buf_;
    RpResource ranking_tile_buf_;
    RpResource shared_data_buf_;
    RpResource normal_tex_;
    RpResource rough_tex_;
    RpResource depth_hierarchy_tex_;
    RpResource prev_tex_;
    RpResource ray_counter_buf_;
    RpResource in_ray_list_buf_;
    RpResource indir_args_buf_;

    RpResource out_color_tex_;
    RpResource out_raylen_tex_;
    RpResource out_ray_list_buf_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
               Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf,
               const char shared_data_buf_name[], const char normal_tex_name[], const char rough_tex_name[],
               const char depth_hierarchy_name[], Ren::WeakTex2DRef prev_tex, const char ray_counter_name[],
               const char in_ray_list_name[], const char indir_args_name[], const char out_color_name[],
               const char out_raylen_name[], const char out_ray_list_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR TRACE HQ"; }
};