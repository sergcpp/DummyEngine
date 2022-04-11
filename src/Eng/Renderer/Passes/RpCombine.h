#pragma once

#include <Ren/RenderPass.h>
#include <Ren/Texture.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

struct RpCombineData {
    RpResRef color_tex;
    RpResRef blur_tex;
    RpResRef exposure_tex;
    RpResRef output_tex;

    bool tonemap = false;
    float gamma = 1.0f, exposure = 1.0f, fade = 0.0f;
};

class RpCombine : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_combine_prog_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const RpCombineData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex);

    Ren::RenderPass render_pass_[Ren::MaxFramesInFlight];
    Ren::Framebuffer output_fb_[Ren::MaxFramesInFlight];

  public:
    RpCombine(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(const ViewState *view_state, const RpCombineData *pass_data) {
        view_state_ = view_state;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};