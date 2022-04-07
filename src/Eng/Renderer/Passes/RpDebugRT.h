#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;
struct ViewState;

struct RpDebugRTData {
    RpResRef shared_data;
    RpResRef geo_data_buf;
    RpResRef materials_buf;
    RpResRef vtx_buf1;
    RpResRef vtx_buf2;
    RpResRef ndx_buf;
    RpResRef env_tex;
    RpResRef lm_tex[5];
    RpResRef dummy_black;

    RpResRef output_tex;
};

class RpDebugRT : public RenderPassExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_debug_rt_;

    // temp data (valid only between Setup and Execute calls)
    uint64_t render_flags_ = 0;
    const ViewState *view_state_ = nullptr;
    const Ren::Camera *draw_cam_ = nullptr;
    const AccelerationStructureData *acc_struct_data_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;
    int depth_w_ = 0, depth_h_ = 0;

    const RpDebugRTData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const DrawList &list,
                          const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
               const RpDebugRTData *pass_data) {
        render_flags_ = list.render_flags;
        view_state_ = view_state;
        draw_cam_ = &list.draw_cam;
        acc_struct_data_ = acc_struct_data;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};