#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpSSAO : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ao_prog_, blit_bilateral_prog_, blit_upscale_prog_,
        blit_upscale_ms_prog_;
    Ren::Tex2DRef dummy_white_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle rand2d_dirs_4x4_tex_;
    const ViewState *view_state_ = nullptr;
    int orphan_index_ = -1;

    RpResource shared_data_buf_;

    RpResource depth_tex_;
    RpResource depth_down_2x_tex_;
    RpResource ssao1_tex_, ssao2_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &ssao1_tex,
                  RpAllocTex &ssao2_tex, RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer ssao_buf1_fb_, ssao_buf2_fb_, output_fb_;
#endif
  public:
    RpSSAO(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               Ren::TexHandle rand2d_dirs_4x4_tex, const char shared_data_buf[],
               const char depth_down_2x[], const char depth_tex[],
               const char output_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSAO PASS"; }
};