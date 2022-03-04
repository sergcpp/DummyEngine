#pragma once

#include <Ren/Texture.h>
#include <Ren/VertexInput.h>

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;
struct ViewState;

class RpRTReflections : public RenderPassBase {
    bool initialized = false;

    // lazily initialized data
    Ren::Pipeline pi_rt_reflections_;
    Ren::Pipeline pi_rt_reflections_inline_;

    // temp data (valid only between Setup and Execute calls)
    uint32_t render_flags_ = 0;
    const ViewState *view_state_ = nullptr;
    const Ren::Camera *draw_cam_ = nullptr;
    const AccelerationStructureData *acc_struct_data_ = nullptr;
    const BindlessTextureData *bindless_tex_ = nullptr;

    RpResource sobol_buf_;
    RpResource scrambling_tile_buf_;
    RpResource ranking_tile_buf_;
    RpResource geo_data_buf_;
    RpResource materials_buf_;
    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;
    RpResource shared_data_buf_;
    RpResource depth_tex_;
    RpResource normal_tex_;
    RpResource env_tex_;
    RpResource lm_tex_[5];
    RpResource dummy_black_;
    RpResource ray_counter_buf_;
    RpResource ray_list_buf_;
    RpResource indir_args_buf_;

    RpResource out_refl_tex_;
    RpResource out_raylen_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
               Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf, const DrawList &list,
               const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
               const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
               const Ren::BufferRef &materials_buf, const char shared_data_buf_name[], const char depth_tex[],
               const char normal_tex[], const Ren::Tex2DRef &dummy_black, const char ray_counter_name[],
               const char ray_list_name[], const char indir_args_name[], const char out_raylen_name[]);

    void ExecuteRTPipeline(RpBuilder &builder);
    void ExecuteRTInline(RpBuilder &builder);

  public:
    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
               Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf, const DrawList &list,
               const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
               const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
               const Ren::BufferRef &materials_buf, const char shared_data_buf_name[], const char depth_tex[],
               const char normal_tex[], const Ren::Tex2DRef &dummy_black, const char ray_counter_name[],
               const char ray_list_name[], const char indir_args_name[], const char out_refl_tex_name[],
               const char out_raylen_name[]);
    void Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakBufferRef sobol_buf,
               Ren::WeakBufferRef scrambling_tile_buf, Ren::WeakBufferRef ranking_tile_buf, const DrawList &list,
               const Ren::BufferRef &vtx_buf1, const Ren::BufferRef &vtx_buf2, const Ren::BufferRef &ndx_buf,
               const AccelerationStructureData *acc_struct_data, const BindlessTextureData *bindless_tex,
               const Ren::BufferRef &materials_buf, const char shared_data_buf_name[], const char depth_tex[],
               const char normal_tex[], const Ren::Tex2DRef &dummy_black, const char ray_counter_name[],
               const char ray_list_name[], const char indir_args_name[], Ren::WeakTex2DRef out_refl_tex,
               const char out_raylen_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "RT REFLECTIONS"; }
};