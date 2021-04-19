#include "RpResolve.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpResolve::Setup(RpBuilder &builder, const ViewState *view_state,
                      const char color_tex[], const char output_tex_name[]) {
    view_state_ = view_state;

    color_tex_ = builder.ReadTexture(color_tex, *this);

    {
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, *this);
    }
}

void RpResolve::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2DMs, REN_BASE0_TEX_SLOT, color_tex.ref->handle()}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                       float(view_state_->act_res[1])}}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {resolve_fb_.id(), 0},
                        blit_ms_resolve_prog_.get(), bindings, 1, uniforms, 1);
}

void RpResolve::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ms_resolve_prog_ =
            sh.LoadProgram(ctx, "blit_ms_resolve", "internal/blit.vert.glsl",
                           "internal/blit_ms_resolve.frag.glsl");
        assert(blit_ms_resolve_prog_->ready());

        initialized = true;
    }

    if (!resolve_fb_.Setup(output_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpResolve: resolve_fb_ init failed!");
    }
}
