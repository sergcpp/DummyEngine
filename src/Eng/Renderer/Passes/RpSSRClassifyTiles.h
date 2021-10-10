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

    RpResource spec_tex_;
    RpResource temp_variance_mask_buf_;

    RpResource tile_metadata_mask_buf_;
    RpResource ray_counter_buf_;
    RpResource ray_list_buf_;
    RpResource rough_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char spec_tex_name[],
               const char temp_variance_mask_name[], const char tile_metadata_mask_name[],
               const char ray_counter_name[], const char ray_list_name[], const char rough_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR CLASSIFY TILES"; }
};
