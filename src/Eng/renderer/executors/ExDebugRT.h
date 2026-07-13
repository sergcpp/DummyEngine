#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class ShaderLoader;
class ExDebugRT final : public FgExecutor {
  public:
    struct Args {
        FgBufROHandle shared_data;
        FgBufROHandle geo_data;
        FgBufROHandle materials;
        FgBufROHandle vtx_buf1;
        FgBufROHandle vtx_buf2;
        FgBufROHandle ndx_buf;
        FgImgROHandle env;
        FgBufROHandle lights;
        FgImgROHandle shadow_depth, shadow_color;
        FgImgROHandle ltc_luts;
        FgBufROHandle cells;
        FgBufROHandle items;
        FgBufROHandle tlas_buf;

        FgImgROHandle irradiance;
        FgImgROHandle distance;
        FgImgROHandle offset;

        FgBufROHandle cache_entries;
        FgBufROHandle cache_voxels;

        Ren::AccStructROHandle tlas;
        uint32_t cull_mask = 0xffffffff;

        struct {
            uint32_t root_node = 0xffffffff;
            FgBufROHandle rt_blas;
            FgBufROHandle prim_ndx;
            FgBufROHandle mesh_instances;
        } swrt;

        FgImgRWHandle output;
    };

    ExDebugRT(ShaderLoader &sh, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
              const Args *args);

    void Execute(const FgContext &fg) override;

  private:
    Ren::PipelineHandle pi_debug_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void Execute_HWRT(const FgContext &fg);
    void Execute_SWRT(const FgContext &fg);
};
} // namespace Eng