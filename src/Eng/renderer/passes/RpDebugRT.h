#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../graph/SubPass.h"
#include "../Renderer_DrawList.h"

struct ViewState;

namespace Eng {
class PrimDraw;
struct RpDebugRTData {
    RpResRef shared_data;
    RpResRef geo_data_buf;
    RpResRef materials_buf;
    RpResRef vtx_buf1;
    RpResRef vtx_buf2;
    RpResRef ndx_buf;
    RpResRef env_tex;
    RpResRef lights_buf;
    RpResRef shadowmap_tex;
    RpResRef ltc_luts_tex;
    RpResRef lm_tex[5];
    RpResRef dummy_black;

    struct {
        uint32_t root_node;
        RpResRef rt_blas_buf;
        RpResRef prim_ndx_buf;
        RpResRef meshes_buf;
        RpResRef mesh_instances_buf;
        RpResRef rt_tlas_buf;
        RpResRef textures_buf;
    } swrt;

    RpResRef output_tex;
};

class RpDebugRT : public RpExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_debug_hwrt_, pi_debug_swrt_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Ren::IAccStructure *tlas_to_debug_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    int depth_w_ = 0, depth_h_ = 0;

    const RpDebugRTData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh);

    void Execute_HWRT(RpBuilder &builder);
    void Execute_SWRT(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const Ren::IAccStructure *tlas_to_debug,
               const BindlessTextureData *bindless_tex, const RpDebugRTData *pass_data) {
        view_state_ = view_state;
        tlas_to_debug_ = tlas_to_debug;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};
} // namespace Eng