#pragma once

#include "../Graph/GraphBuilder.h"
#include "../Renderer_DrawList.h"

class PrimDraw;

class RpSampleBrightness : public RenderPassBase {
    PrimDraw &prim_draw_;
    Ren::Vec2i res_;
    bool initialized_ = false;
    int cur_offset_ = 0;

    // lazily initialized data
    Ren::ProgramRef blit_red_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer reduced_fb_;

    // temp data (valid only between Setup and Execute calls)
    RpResource input_tex_;

    RpResource reduced_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &reduced_tex);

    Ren::BufferRef readback_buf_;

  public:
    RpSampleBrightness(PrimDraw &prim_draw, Ren::Vec2i res) : prim_draw_(prim_draw), res_(res) {}
    ~RpSampleBrightness() = default;

    void Setup(RpBuilder &builder, Ren::WeakTex2DRef tex_to_sample, const char reduced_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SAMPLE BRIGHTNESS"; }

    Ren::Vec2i res() const { return res_; }
};