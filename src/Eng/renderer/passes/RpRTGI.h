#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../graph/SubPass.h"
#include "../Renderer_DrawList.h"

struct ViewState;

namespace Eng {
class PrimDraw;
struct RpRTGIData {
    RpResRef noise_tex;
    RpResRef geo_data;
    RpResRef materials;
    RpResRef vtx_buf1;
    RpResRef vtx_buf2;
    RpResRef ndx_buf;
    RpResRef shared_data;
    RpResRef depth_tex;
    RpResRef normal_tex;
    // RpResRef flat_normal_tex;
    RpResRef env_tex;
    RpResRef lm_tex[5];
    RpResRef lights_buf;
    RpResRef shadowmap_tex;
    RpResRef ltc_luts_tex;
    RpResRef dummy_black;
    RpResRef ray_counter;
    RpResRef ray_list;
    RpResRef indir_args;
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

    bool two_bounce = false;

    RpResRef out_gi_tex;
};

class RpRTGI : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_gi_;
    Ren::Pipeline pi_rt_gi_inline_, pi_rt_gi_2bounce_inline_;
    Ren::Pipeline pi_rt_gi_swrt_, pi_rt_gi_2bounce_swrt_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const RpRTGIData *pass_data_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT_Pipeline(RpBuilder &builder);
    void Execute_HWRT_Inline(RpBuilder &builder);

    void Execute_SWRT(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const BindlessTextureData *bindless_tex,
               const RpRTGIData *pass_data) {
        view_state_ = view_state;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};
} // namespace Eng