#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

struct ViewState;

namespace Eng {
struct RpSampleLightsData {
    RpResRef shared_data;
    RpResRef random_seq;
    RpResRef lights_buf;

    RpResRef geo_data;
    RpResRef materials;
    RpResRef vtx_buf1;
    RpResRef ndx_buf;
    RpResRef tlas_buf;

    RpResRef albedo_tex;
    RpResRef depth_tex;
    RpResRef norm_tex;
    RpResRef spec_tex;

    Ren::IAccStructure *tlas = nullptr;

    struct {
        uint32_t root_node;
        RpResRef rt_blas_buf;
        RpResRef prim_ndx_buf;
        RpResRef meshes_buf;
        RpResRef mesh_instances_buf;
        RpResRef textures_buf;
    } swrt;

    RpResRef out_diffuse_tex;
    RpResRef out_specular_tex;
};

class RpSampleLights : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_sample_lights_hwrt_;
    Ren::Pipeline pi_sample_lights_swrt_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const RpSampleLightsData *pass_data_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(RpBuilder &builder);

    void Execute_SWRT(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const BindlessTextureData *bindless_tex,
               const RpSampleLightsData *pass_data) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng