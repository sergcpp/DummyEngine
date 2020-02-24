#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpDOF : public Graph::RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_dof_init_coc_prog_, blit_dof_bilateral_prog_,
        blit_dof_calc_near_prog_, blit_dof_small_blur_prog_, blit_dof_combine_prog_,
        blit_dof_combine_ms_prog_, blit_gauss_prog_, blit_down_depth_prog_;

    // temp data (valid only between Setup and Execute calls)
    const Ren::Camera *draw_cam_ = nullptr;
    Ren::TexHandle color_tex_, depth_tex_, down_buf_4x_, down_depth_2x_, down_depth_4x_,
        down_tex_coc_[2], blur_temp_4x_[2];
    Ren::TexHandle output_tex_;
    const ViewState *view_state_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer coc_fb_[2], blur_fb_[2], depth_4x_fb_, dof_fb_;
#endif
  public:
    RpDOF(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(Graph::RpBuilder &builder, const Ren::Camera *draw_cam,
               const ViewState *view_state, Ren::TexHandle color_tex,
               Ren::TexHandle depth_tex, Ren::TexHandle down_buf_4x,
               Ren::TexHandle down_depth_2x, Ren::TexHandle down_depth_4x,
               Ren::TexHandle down_tex_coc[2], Ren::TexHandle blur_temp_4x[2],
               Ren::TexHandle dof_buf, Graph::ResourceHandle in_shared_data_buf,
               Ren::TexHandle output_tex);
    void Execute(Graph::RpBuilder &builder) override;

    const char *name() const override { return "DOF"; }
};