#pragma once

#include "../Graph/GraphBuilder.h"

class PrimDraw;
class ProbeStorage;
struct ViewState;

class RpReflections : public RenderPassBase {
    PrimDraw &prim_draw_;
    bool initialized = false;

    // lazily initialized data
    int orphan_index_ = -1;
    Ren::ProgramRef blit_ssr_prog_, blit_ssr_ms_prog_, blit_ssr_dilate_prog_,
        blit_ssr_compose_prog_, blit_ssr_compose_ms_prog_;

    // temp data (valid only between Setup and Execute calls)
    Ren::TexHandle down_buf_4x_tex_;
    Ren::Tex2DRef brdf_lut_;
    const ViewState *view_state_ = nullptr;
    const ProbeStorage *probe_storage_ = nullptr;

    RpResource shared_data_buf_;
    RpResource cells_buf_;
    RpResource items_buf_;

    RpResource depth_tex_;
    RpResource normal_tex_;
    RpResource spec_tex_;
    RpResource depth_down_2x_tex_;
    RpResource ssr1_tex_, ssr2_tex_;
    RpResource output_tex_;

    void LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &ssr1_tex,
                  RpAllocTex &ssr2_tex, RpAllocTex &output_tex);

#if defined(USE_GL_RENDER)
    Ren::Framebuffer ssr_buf1_fb_, ssr_buf2_fb_, output_fb_;
#endif
  public:
    RpReflections(PrimDraw &prim_draw) : prim_draw_(prim_draw) {}

    void Setup(RpBuilder &builder, const ViewState *view_state, int orphan_index,
               const ProbeStorage *probe_storage, Ren::TexHandle down_buf_4x_tex,
               Ren::Tex2DRef brdf_lut, const char shared_data_buf[],
               const char cells_buf[], const char items_buf[], const char depth_tex[],
               const char normal_tex[], const char spec_tex[], const char depth_down_2x[],
               const char output_tex_name[]);
    void Execute(RpBuilder &builder) override;

    const char *name() const override { return "REFLECTIONS PASS"; }
};