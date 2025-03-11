#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../framegraph/FgNode.h"

struct view_state_t;

namespace Eng {
class PrimDraw;
class ExRTReflections final : public FgExecutor {
  public:
    explicit ExRTReflections(const bool layered) : layered_(layered) {}

    struct Args {
        FgResRef noise_tex;
        FgResRef geo_data;
        FgResRef materials;
        FgResRef vtx_buf1;
        FgResRef vtx_buf2;
        FgResRef ndx_buf;
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef env_tex;
        FgResRef lights_buf;
        FgResRef shadow_depth_tex, shadow_color_tex;
        FgResRef ltc_luts_tex;
        FgResRef cells_buf;
        FgResRef items_buf;
        FgResRef ray_counter;
        FgResRef ray_list;
        FgResRef indir_args;
        FgResRef tlas_buf;

        FgResRef irradiance_tex;
        FgResRef distance_tex;
        FgResRef offset_tex;

        FgResRef stoch_lights_buf;
        FgResRef light_nodes_buf;

        FgResRef oit_depth_buf;

        const Ren::IAccStructure *tlas = nullptr;
        const probe_volume_t *probe_volume = nullptr;

        struct {
            uint32_t root_node = 0xffffffff;
            FgResRef rt_blas_buf;
            FgResRef prim_ndx_buf;
            FgResRef mesh_instances_buf;
            FgResRef textures_buf;
        } swrt;

        bool four_bounces = false;

        FgResRef out_refl_tex[OIT_REFLECTION_LAYERS];
    };

    void Setup(FgBuilder &builder, const view_state_t *view_state, const BindlessTextureData *bindless_tex,
               const Args *args) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        args_ = args;
    }

    void Execute(FgBuilder &builder) override;

  private:
    bool layered_ = false;
    bool initialized_ = false;

    // lazily initialized data
    Ren::PipelineRef pi_rt_reflections_[3], pi_rt_reflections_4bounce_[3];

    // temp data (valid only between Setup and Execute calls)
    const view_state_t *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const Args *args_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(FgBuilder &builder);
    void Execute_SWRT(FgBuilder &builder);
};
} // namespace Eng