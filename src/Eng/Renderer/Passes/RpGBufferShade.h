#pragma once
#if 0
#include <Ren/Pipeline.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpGBufferShade : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_gbuf_shade_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;
    RpResource lights_buf_;
    RpResource decals_buf_;
    RpResource depth_tex_;
    RpResource albedo_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource shad_tex_;
    RpResource ssao_tex_;
    
    RpResource out_color_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[], const char cells_buf[],
               const char items_buf[], const char lights_buf[], const char decals_buf[], const char depth_tex_name[],
               const char albedo_tex_name[], const char normal_tex_name[], const char spec_tex_name[],
               const char shadowmap_tex[], const char ssao_tex[], const char out_color_img_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "GBUFFER SHADE"; }
};
#endif