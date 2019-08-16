#pragma once

#include <Ren/Texture.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpResolve : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ms_resolve_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    RpResource color_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer resolve_fb_;
#endif
  public:
    RpResolve(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const char color_tex[], const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "REFLECTIONS PASS"; }
};