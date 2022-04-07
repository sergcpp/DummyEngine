#pragma once

#include <Ren/RenderPass.h>
#include <Ren/Texture.h>

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

struct RpCombineData {
    RpResource color_tex;
    RpResource blur_tex;
    RpResource output_tex;
};

class RpCombine : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;
    bool tonemap_ = false;
    float gamma_ = 1.0f, exposure_ = 1.0f, fade_ = 0.0f;

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

    void Setup(const ViewState *view_state, const float gamma, const float exposure, const float fade,
               const bool tonemap, const RpCombineData *pass_data) {
        view_state_ = view_state;
        gamma_ = gamma;
        exposure_ = exposure;
        fade_ = fade;
        tonemap_ = tonemap;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};