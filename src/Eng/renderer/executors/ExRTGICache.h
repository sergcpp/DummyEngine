#pragma once

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

namespace Eng {
class ExRTGICache final : public FgExecutor {
  public:
    struct Args {
        FgResRef geo_data;
        FgResRef materials;
        FgResRef vtx_buf1;
        FgResRef vtx_buf2;
        FgResRef ndx_buf;
        FgResRef shared_data;
        FgResRef env_tex;
        FgResRef lights_buf;
        FgResRef shadowmap_tex;
        FgResRef ltc_luts_tex;
        FgResRef cells_buf;
        FgResRef items_buf;
        FgResRef tlas_buf; // fake read for now

        Ren::IAccStructure *tlas = nullptr;

        struct {
            uint32_t root_node = 0xffffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef meshes_buf;
            FgResRef mesh_instances_buf;
            FgResRef textures_buf;
        } swrt;

        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef offset_tex;

        FgResRef random_seq;
        FgResRef stoch_lights_buf;
        FgResRef light_nodes_buf;

        FgResRef out_ray_data_tex;

        const ViewState *view_state = nullptr;
        Ren::Span<const ProbeVolume> probe_volumes;
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
    Ren::Pipeline pi_rt_gi_cache_[2];

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng