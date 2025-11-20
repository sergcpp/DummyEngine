#pragma once

#include <Ren/Image.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class PrimDraw;
class ExDebugRT final : public FgExecutor {
  public:
    struct Args {
        FgResRef shared_data;
        FgResRef geo_data_buf;
        FgResRef materials_buf;
        FgResRef vtx_buf1;
        FgResRef vtx_buf2;
        FgResRef ndx_buf;
        FgResRef env_tex;
        FgResRef lights_buf;
        FgResRef shadow_depth_tex, shadow_color_tex;
        FgResRef ltc_luts_tex;
        FgResRef cells_buf;
        FgResRef items_buf;
        FgResRef tlas_buf;

        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef offset_tex;

        const Ren::IAccStructure *tlas = nullptr;
        uint32_t cull_mask = 0xffffffff;

        struct {
            uint32_t root_node = 0xffffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
        } swrt;

        FgResRef output_tex;
    };

    ExDebugRT(FgContext &fg, const view_state_t *view_state, const BindlessTextureData *bindless_tex, const Args *args);

    void Execute(FgContext &fg) override;

  private:
    Ren::PipelineRef pi_debug_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    int depth_w_ = 0, depth_h_ = 0;

    const Args *args_ = nullptr;

    void Execute_HWRT(FgContext &fg);
    void Execute_SWRT(FgContext &fg);
};
} // namespace Eng