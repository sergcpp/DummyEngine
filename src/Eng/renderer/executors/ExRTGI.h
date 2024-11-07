#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct ViewState;

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
        FgResRef env_tex;
        FgResRef lights_buf;
        FgResRef shadowmap_tex;
        FgResRef ltc_luts_tex;
        FgResRef cells_buf;
        FgResRef items_buf;
        FgResRef ray_counter;
        FgResRef ray_list;
        FgResRef indir_args;
        FgResRef tlas_buf; // fake read for now

        FgResRef stoch_lights_buf;
        FgResRef light_nodes_buf;

        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef offset_tex;

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node = 0xffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
            FgResRef textures_buf;
        } swrt;

        bool two_bounce = false;

        FgResRef out_gi_tex;
    };

    void Setup(FgBuilder &builder, const ViewState *view_state, const BindlessTextureData *bindless_tex,
               const Args *args) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        args_ = args;
    }

    void Execute(FgBuilder &builder) override;

  private:
    bool initialized_ = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_gi_[2], pi_rt_gi_2bounce_[2];

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng