#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

struct DrawList;

class RpSSRClassifyTiles : public RenderPassBase {
    // lazily initialized data
    bool initialized = false;
    Ren::Pipeline pi_classify_tiles_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    float glossy_thres_ = 1.0f, mirror_thres_ = 0.0f;
    int sample_count_ = 4;
    bool variance_guided_ = false;

    RpResource depth_tex_;
    RpResource norm_tex_;
    RpResource variance_history_tex_;

    RpResource ray_counter_buf_;
    RpResource ray_list_buf_;
    RpResource tile_list_buf_;
    RpResource refl_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, float glossy_thres, float mirror_thres,
               int sample_count, bool variance_guided, const char depth_tex_name[], const char norm_tex_name[],
               Ren::WeakTex2DRef variance_history_tex, const char ray_counter_name[], const char ray_list_name[],
               const char tile_list_name[], const char refl_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR CLASSIFY"; }
};
