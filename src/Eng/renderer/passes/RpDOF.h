#pragma once
#if 0
#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpDOF : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_dof_init_coc_prog_, blit_dof_bilateral_prog_, blit_dof_calc_near_prog_,
        blit_dof_small_blur_prog_, blit_dof_combine_prog_, blit_dof_combine_ms_prog_, blit_gauss_prog_,
        blit_down_depth_prog_;

    // temp data (valid only between Setup and Execute calls)
    const Ren::Camera *draw_cam_ = nullptr;
    Ren::WeakTex2DRef down_buf_4x_;
    const ViewState *view_state_ = nullptr;

    RpResource shared_data_buf_;
    RpResource color_tex_;
    RpResource depth_tex_;
    RpResource down_depth_2x_tex_, down_depth_4x_tex_;

    RpResource blur_temp_4x_[2], down_tex_coc_[2];
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh, RpAllocTex &down_depth_4x_tex, RpAllocTex &blur1_temp_4x,
                  RpAllocTex &blur2_temp_4x, RpAllocTex &coc1_tex, RpAllocTex &coc2_tex, RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer coc_fb_[2], blur_fb_[2], depth_4x_fb_, dof_fb_;
#endif
  public:
    RpDOF(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const Ren::Camera *draw_cam, const ViewState *view_state,
               const char shared_data_buf[], const char color_tex_name[], const char depth_tex_name[],
               const char depth_down_2x_name[], const char depth_down_4x_name[], Ren::WeakTex2DRef down_buf_4x,
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "DOF"; }
};
#endif