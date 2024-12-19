#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

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
        FgResRef shadowmap_tex;
        FgResRef ltc_luts_tex;
        FgResRef cells_buf;
        FgResRef items_buf;

        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef offset_tex;

        struct {
            uint32_t root_node;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
            FgResRef rt_tlas_buf;
            FgResRef textures_buf;
        } swrt;

        FgResRef output_tex;
    };

    ExDebugRT(FgBuilder &builder, const ViewState *view_state, const Ren::IAccStructure *tlas_to_debug,
              const BindlessTextureData *bindless_tex, const Args *args);

    void Execute(FgBuilder &builder) override;

  private:
    Ren::PipelineRef pi_debug_hwrt_, pi_debug_swrt_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Ren::IAccStructure *tlas_to_debug_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    int depth_w_ = 0, depth_h_ = 0;

    const Args *args_ = nullptr;

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng