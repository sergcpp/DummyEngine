#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

struct RpTAAData {
    RpResRef shared_data;

    RpResRef clean_tex;
    RpResRef depth_tex;
    RpResRef velocity_tex;
    RpResRef history_tex;

    RpResRef output_tex;
};

class RpTAA : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_taa_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer resolve_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    float reduced_average_ = 0.0f, max_exposure_ = 1.0f;

    const RpTAAData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex,
                  RpAllocTex &history_tex, RpAllocTex &output_tex);

  public:
    RpTAA(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const float reduced_average, const float max_exposure,
               const RpTAAData *pass_data) {
        view_state_ = view_state;

        reduced_average_ = reduced_average;
        max_exposure_ = max_exposure;

        pass_data_ = pass_data;
    }

    void Execute(RpBuilder &builder) override;
};