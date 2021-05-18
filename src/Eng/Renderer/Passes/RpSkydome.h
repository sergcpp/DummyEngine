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
    const ViewState *view_state_ = nullptr;
    const EnvironmentWeak *env_ = nullptr;

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::MeshRef skydome_mesh_;
#if defined(USE_GL_RENDER)
    Ren::Vao skydome_vao_;
    Ren::Framebuffer cached_fb_;
#endif

    RpResource shared_data_buf_;

    RpResource color_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &color_tex,
                  RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawSkydome(RpBuilder &builder);

  public:
    ~RpSkydome();

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state,
               int orphan_index, const char shared_data_buf[], const char color_tex[],
               const char spec_tex[], const char depth_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKYDOME"; }
};