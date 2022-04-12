#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;
struct ViewState;

struct RpRTReflectionsData {
    RpResRef noise_tex;
    RpResRef geo_data;
    RpResRef materials;
    RpResRef vtx_buf1;
    RpResRef vtx_buf2;
    RpResRef ndx_buf;
    RpResRef shared_data;
    RpResRef depth_tex;
    RpResRef normal_tex;
    RpResRef env_tex;
    RpResRef lm_tex[5];
    RpResRef dummy_black;
    RpResRef ray_counter;
    RpResRef ray_list;
    RpResRef indir_args;

    RpResRef out_refl_tex;
    RpResRef out_raylen_tex;
};

class RpRTReflections : public RenderPassExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_reflections_;
    Ren::Pipeline pi_rt_reflections_inline_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const AccelerationStructureData *acc_struct_data_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const RpRTReflectionsData *pass_data_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

    void ExecuteRTPipeline(RpBuilder &builder);
    void ExecuteRTInline(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const AccelerationStructureData *acc_struct_data,
               const BindlessTextureData *bindless_tex, const RpRTReflectionsData *pass_data) {
        view_state_ = view_state;
        acc_struct_data_ = acc_struct_data;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};