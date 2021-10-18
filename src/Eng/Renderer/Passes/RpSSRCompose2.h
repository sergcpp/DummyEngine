#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
struct ViewState;

class RpSSRCompose2 : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    Ren::ProgramRef blit_ssr_compose_prog_;
    Ren::RenderPass render_pass_;
    Ren::Framebuffer output_fb_;

    // temp data (valid only between Setup and Execute calls)
    const ViewState *view_state_ = nullptr;
    const Ren::ProbeStorage *probe_storage_ = nullptr;

    RpResource shared_data_buf_;
    RpResource depth_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource refl_tex_;
    RpResource brdf_lut_;

    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex);

  public:
    RpSSRCompose2(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, const Ren::ProbeStorage *probe_storage,
               Ren::Tex2DRef brdf_lut, const char shared_data_buf[], const char depth_tex_name[],
               const char normal_tex_name[], const char spec_tex_name[], const char refl_tex_name[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "SSR COMPOSE"; }
};