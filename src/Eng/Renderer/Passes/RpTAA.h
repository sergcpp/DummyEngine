#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpTAA : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_static_vel_prog_, blit_taa_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle history_tex_;
    const ViewState *view_state_ = nullptr;
    int orphan_index_ = -1;
    float reduced_average_ = 0.0f, max_exposure_ = 1.0f;

    RpResource shared_data_buf_;

    RpResource clean_tex_;
    RpResource depth_tex_;
    RpResource velocity_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex,
                  RpAllocTex &velocity_tex, RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer velocity_fb_, resolve_fb_;
#endif
  public:
    RpTAA(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               Ren::TexHandle history_tex, float reduced_average, float max_exposure,
               const char shared_data_buf[], const char color_tex[],
               const char depth_tex[], const char velocity_tex[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "TAA"; }
};