#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;
struct ViewState;

struct RpRTReflectionsData {
    RpResource sobol, scrambling_tile, ranking_tile;
    RpResource geo_data;
    RpResource materials;
    RpResource vtx_buf1;
    RpResource vtx_buf2;
    RpResource ndx_buf;
    RpResource shared_data;
    RpResource depth_tex;
    RpResource normal_tex;
    RpResource env_tex;
    RpResource lm_tex[5];
    RpResource dummy_black;
    RpResource ray_counter;
    RpResource ray_list;
    RpResource indir_args;

    RpResource out_refl_tex;
    RpResource out_raylen_tex;
};

class RpRTReflections : public RenderPassExecutor {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_reflections_;
    Ren::Pipeline pi_rt_reflections_inline_;

    // temp data (valid only between Setup and Execute calls)
    uint64_t render_flags_ = 0;
    const ViewState *view_state_ = nullptr;
    const Ren::Camera *draw_cam_ = nullptr;
    const AccelerationStructureData *acc_struct_data_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    const RpRTReflectionsData *pass_data_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

    void ExecuteRTPipeline(RpBuilder &builder);
    void ExecuteRTInline(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, const DrawList &list,
               const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
               const RpRTReflectionsData *pass_data) {
        render_flags_ = list.render_flags;
        view_state_ = view_state;
        draw_cam_ = &list.draw_cam;
        acc_struct_data_ = acc_struct_data;
        bindless_tex_ = bindless_tex;
        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};