#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class ExRTGI final : public FgExecutor {
  public:
    struct Args {
        FgResRef noise_tex;
        FgResRef geo_data;
        FgResRef materials;
        FgResRef vtx_buf1;
        FgResRef ndx_buf;
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef ray_counter;
        FgResRef ray_list;
        FgResRef indir_args;
        FgResRef tlas_buf; // fake read for now

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node = 0xffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
        } swrt;

        bool second_bounce = false;

        FgResRef out_ray_hits_buf;
    };

    ExRTGI(const view_state_t *view_state, const BindlessTextureData *bindless_tex, const Args *args)
        : view_state_(view_state), bindless_tex_(bindless_tex), args_(args) {}

    void Execute(FgContext &fg) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineRef pi_rt_gi_;

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgContext &fg);
    void Execute_HWRT_Pipeline(FgContext &fg);
    void Execute_SWRT(FgContext &fg);
};
} // namespace Eng