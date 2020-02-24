#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class PrimDraw;

class RpDebugProbes : public Graph::RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;
    float debug_roughness_ = 0.0f;

    // temp data (valid only between Setup and Execute calls)
    const ProbeStorage *probe_storage_ = nullptr;
    DynArrayConstRef<ProbeItem> probes_;
    const ViewState *view_state_ = nullptr;
    Ren::TexHandle output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
    void DrawProbes(Graph::RpBuilder &builder);

    // lazily initialized data
#if defined(USE_GL_RENDER)
    Ren::ProgramRef probe_prog_;
    Ren::Framebuffer draw_fb_;
#endif
  public:
    RpDebugProbes(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(Graph::RpBuilder &builder, const DrawList &list,
               const ViewState *view_state, Graph::ResourceHandle in_shared_data_buf,
               Ren::TexHandle output_tex);
    void Execute(Graph::RpBuilder &builder) override;

    // TODO: remove this
    int alpha_blend_start_index_ = -1;

    const char *name() const override { return "DEBUG PROBES"; }
};