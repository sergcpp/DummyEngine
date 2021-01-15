#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/RastState.h>
#if defined(USE_GL_RENDER)
#include <Ren/VaoGL.h>
#endif

class RpSkydome : public RenderPassBase {
    bool initialized = false;
    Ren::ProgramRef skydome_prog_;

    int orphan_index_ = 0;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle color_tex_, spec_tex_, depth_tex_;
    const ViewState *view_state_ = nullptr;
    const Environment *env_ = nullptr;

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::MeshRef skydome_mesh_;
#if defined(USE_GL_RENDER)
    Ren::Vao skydome_vao_;
    Ren::Framebuffer cached_fb_;
#endif

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);
    void DrawSkydome(RpBuilder &builder);

  public:
    ~RpSkydome();

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               int orphan_index, Ren::TexHandle color_tex, Ren::TexHandle spec_tex,
               Ren::TexHandle depth_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKYDOME"; }
};