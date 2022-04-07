#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

struct RpSSRComposeData {
    RpResource shared_data;
    RpResource cells_buf;
    RpResource items_buf;

    RpResource depth_tex;
    RpResource normal_tex;
    RpResource spec_tex;
    RpResource depth_down_2x_tex;
    RpResource down_buf_4x_tex;
    RpResource ssr_tex;
    RpResource brdf_lut;

    RpResource output_tex;
};

class RpSSRCompose : public RenderPassExecutor {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_compose_prog_, blit_ssr_compose_ms_prog_;
    Ren::ProgramRef blit_ssr_compose_hq_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Ren::ProbeStorage *probe_storage_ = nullptr;
    const RpSSRComposeData *pass_data_ = nullptr;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSRCompose(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const Ren::ProbeStorage *probe_storage,
               const RpSSRComposeData *pass_data) {
        view_state_ = view_state;
        probe_storage_ = probe_storage;
        pass_data_ = pass_data;
    }
    void Execute(RpBuilder &builder) override;
};
