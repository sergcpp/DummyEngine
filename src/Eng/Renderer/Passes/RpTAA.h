#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;

class RpTAA : public Graph::RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_static_vel_prog_, blit_taa_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle depth_tex_, clean_tex_, history_tex_, velocity_tex_;
    Ren::TexHandle output_tex_;
    const ViewState *view_state_ = nullptr;
    float reduced_average_ = 0.0f, max_exposure_ = 1.0f;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer velocity_fb_, resolve_fb_;
#endif
  public:
    RpTAA(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(Graph::RpBuilder &builder, const ViewState *view_state,
               Ren::TexHandle depth_tex, Ren::TexHandle clean_tex,
               Ren::TexHandle history_tex, Ren::TexHandle velocity_tex,
               float reduced_average, float max_exposure,
               Graph::ResourceHandle in_shared_data_buf, Ren::TexHandle output_tex);
    void Execute(Graph::RpBuilder &builder) override;

    const char *name() const override { return "TAA"; }
};