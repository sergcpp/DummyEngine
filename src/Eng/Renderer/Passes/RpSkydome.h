#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

#include <Ren/Pipeline.h>
#include <Ren/RastState.h>
#include <Ren/RenderPass.h>
#include <Ren/VertexInput.h>

class RpSkydome : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;

    Ren::Vec3f draw_cam_pos_;

    // lazily initialized data
    Ren::RenderPass render_pass_;
    Ren::VertexInput vtx_input_;
    Ren::Pipeline pipeline_;
    Ren::Framebuffer framebuf_[Ren::MaxFramesInFlight];

    RpResource shared_data_buf_;
    RpResource env_tex_;
    RpResource vtx_buf1_;
    RpResource vtx_buf2_;
    RpResource ndx_buf_;

    RpResource color_tex_;
    RpResource spec_tex_;
    RpResource depth_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                  RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);
    void DrawSkydome(RpBuilder &builder, RpAllocBuf &vtx_buf1, RpAllocBuf &vtx_buf2, RpAllocBuf &ndx_buf,
                     RpAllocTex &color_tex, RpAllocTex &spec_tex, RpAllocTex &depth_tex);

  public:
    RpSkydome(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}
    ~RpSkydome();

    void Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, Ren::BufferRef vtx_buf1,
               Ren::BufferRef vtx_buf2, Ren::BufferRef ndx_buf, const char shared_data_buf[], const char color_tex[],
               const char spec_tex[], const char depth_tex[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SKYDOME"; }
};