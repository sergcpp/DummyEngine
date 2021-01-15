#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;

class RpSampleBrightness : public RenderPassBase {
    PrimDraw &prim_draw_;
    Ren::Vec2i res_;
    int frame_sync_window_;
    bool initialized_ = false;
    int cur_offset_ = 0;
    float reduced_average_ = 0.0f;
    int cur_reduce_pbo_ = 0;

    // lazily initialized data
    Ren::ProgramRef blit_red_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle tex_to_sample_, reduce_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer reduced_fb_;
    uint32_t reduce_pbo_[8] = {};

    void InitPBO();
    void DestroyPBO();
#endif
  public:
    RpSampleBrightness(PrimDraw &prim_draw, Ren::Vec2i res, int frame_sync_window)
        : prim_draw_(prim_draw), res_(res), frame_sync_window_(frame_sync_window) {}
    ~RpSampleBrightness() { DestroyPBO(); }

    void Setup(RpBuilder &builder, Ren::TexHandle tex_to_sample,
               Ren::TexHandle reduce_tex);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SAMPLE BRIGHTNESS"; }
    float reduced_average() const { return reduced_average_; }

    Ren::Vec2i res() const { return res_; }
};