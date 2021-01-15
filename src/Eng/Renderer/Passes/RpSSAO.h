#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;

class RpSSAO : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ao_prog_, blit_bilateral_prog_, blit_upscale_prog_,
        blit_upscale_ms_prog_;
    Ren::Tex2DRef dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle depth_tex_, down_depth_2x_tex_, rand2d_dirs_4x4_tex_;
    Ren::TexHandle ssao_buf1_tex_, ssao_buf2_tex_, output_tex_;
    const ViewState *view_state_ = nullptr;
    int orphan_index_ = -1;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer ssao_buf1_fb_, ssao_buf2_fb_, output_fb_;
#endif
  public:
    RpSSAO(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               Ren::TexHandle depth_tex, Ren::TexHandle down_depth_2x_tex,
               Ren::TexHandle rand2d_dirs_4x4_tex, Ren::TexHandle ssao_buf1_tex,
               Ren::TexHandle ssao_buf2_tex, Ren::TexHandle output_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSAO PASS"; }
};