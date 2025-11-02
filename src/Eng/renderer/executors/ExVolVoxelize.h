#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class ExVolVoxelize final : public FgExecutor {
  public:
    struct Args {
        FgResRef shared_data;
        FgResRef stbn_tex;
        FgResRef geo_data;
        FgResRef materials;
        FgResRef tlas_buf;

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node = 0xffffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
            FgResRef vtx_buf1;
            FgResRef ndx_buf;
        } swrt;

        FgResRef out_emission_tex;
        FgResRef out_scatter_tex;
    };

    ExVolVoxelize(const DrawList **p_list, const view_state_t *view_state, const Args *args)
        : p_list_(p_list), view_state_(view_state), args_(args) {}

    void Execute(FgContext &ctx) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineRef pi_vol_voxelize_;

    // temp data (valid only between Setup and Execute calls)
    const DrawList **p_list_ = nullptr;
    const view_state_t *view_state_ = nullptr;
    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgContext &ctx);
    void Execute_SWRT(FgContext &ctx);
};
} // namespace Eng