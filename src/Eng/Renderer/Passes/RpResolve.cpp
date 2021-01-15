#include "RpResolve.h"

#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpResolve::Setup(RpBuilder &builder, const ViewState *view_state,
                      Ren::TexHandle color_tex, Ren::TexHandle output_tex) {
    view_state_ = view_state;
    color_tex_ = color_tex;
    output_tex_ = output_tex;

    // input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 0;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpResolve::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT, color_tex_}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                       float(view_state_->act_res[1])}}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {resolve_fb_.id(), 0},
                        blit_ms_resolve_prog_.get(), bindings, 1, uniforms, 1);
}

void RpResolve::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_ms_resolve_prog_ =
            sh.LoadProgram(ctx, "blit_ms_resolve", "internal/blit.vert.glsl",
                           "internal/blit_ms_resolve.frag.glsl");
        assert(blit_ms_resolve_prog_->ready());

        initialized = true;
    }

    if (!resolve_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpResolve: resolve_fb_ init failed!");
    }
}
