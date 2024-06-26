#pragma once

#include "../Renderer_DrawList.h"
#include "../graph/SubPass.h"

namespace Eng {
struct RpRTGICacheData {
    RpResRef geo_data;
    RpResRef materials;
    RpResRef vtx_buf1;
    RpResRef vtx_buf2;
    RpResRef ndx_buf;
    RpResRef shared_data;
    RpResRef env_tex;
    RpResRef lights_buf;
    RpResRef shadowmap_tex;
    RpResRef ltc_luts_tex;
    RpResRef cells_buf;
    RpResRef items_buf;
    RpResRef tlas_buf; // fake read for now

    Ren::IAccStructure *tlas = nullptr;

    struct {
        uint32_t root_node;
        RpResRef rt_blas_buf;
        RpResRef prim_ndx_buf;
        RpResRef meshes_buf;
        RpResRef mesh_instances_buf;
        RpResRef textures_buf;
    } swrt;

    RpResRef irradiance_tex;
    RpResRef distance_tex;
    RpResRef offset_tex;

    RpResRef out_ray_data_tex;

    const ProbeVolume *probe_volume = nullptr;
};

class RpRTGICache : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_gi_cache_;
    Ren::Pipeline pi_rt_gi_cache_inline_;
    Ren::Pipeline pi_rt_gi_cache_swrt_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const RpRTGICacheData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    //void Execute_HWRT_Pipeline(RpBuilder &builder);
    void Execute_HWRT_Inline(RpBuilder &builder);

    void Execute_SWRT(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const BindlessTextureData *bindless_tex,
               const RpRTGICacheData *pass_data) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng