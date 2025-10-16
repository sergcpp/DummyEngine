#include "ExPostprocess.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/blit_postprocess_interface.h"

Eng::ExPostprocess::ExPostprocess(PrimDraw &prim_draw, ShaderLoader &sh, const view_state_t *view_state,
                                  const Args *args)
    : prim_draw_(prim_draw) {
    view_state_ = view_state;
    args_ = args;

    blit_postprocess_prog_[0][0] = sh.LoadProgram("internal/blit_postprocess.vert.glsl",
                                                  "internal/blit_postprocess@ABERRATION;PURKINJE.frag.glsl");
    blit_postprocess_prog_[0][1] = sh.LoadProgram(
        "internal/blit_postprocess.vert.glsl", "internal/blit_postprocess@ABERRATION;PURKINJE;TWO_TARGETS.frag.glsl");
    blit_postprocess_prog_[1][0] = sh.LoadProgram("internal/blit_postprocess.vert.glsl",
                                                  "internal/blit_postprocess@ABERRATION;PURKINJE;LUT.frag.glsl");
    blit_postprocess_prog_[1][1] = sh.LoadProgram("internal/blit_postprocess.vert.glsl",
                                                  "internal/blit_postprocess@ABERRATION;LUT;TWO_TARGETS.frag.glsl");
}

void Eng::ExPostprocess::Execute(FgContext &fg) {
    FgAllocTex &exposure_tex = fg.AccessROTexture(args_->exposure_tex);
    FgAllocTex &color_tex = fg.AccessROTexture(args_->color_tex);
    FgAllocTex &bloom_tex = fg.AccessROTexture(args_->bloom_tex);
    FgAllocTex &output_tex = fg.AccessRWTexture(args_->output_tex);
    FgAllocTex *lut_tex = nullptr, *output_tex2 = nullptr;
    if (args_->lut_tex) {
        lut_tex = &fg.AccessROTexture(args_->lut_tex);
    }
    if (args_->output_tex2) {
        output_tex2 = &fg.AccessRWTexture(args_->output_tex2);
    }

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->out_res[0];
    rast_state.viewport[3] = view_state_->out_res[1];

    BlitPostprocess::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
    uniform_params.tex_size = Ren::Vec2f{float(view_state_->out_res[0]), float(view_state_->out_res[1])};
    uniform_params.tonemap_mode = float(args_->tonemap_mode);
    uniform_params.inv_gamma = args_->inv_gamma;
    uniform_params.fade = args_->fade;
    uniform_params.aberration = args_->aberration;
    uniform_params.purkinje = args_->purkinje;
    uniform_params.pre_exposure = view_state_->pre_exposure;

    Ren::SmallVector<Ren::Binding, 8> bindings = {
        {Ren::eBindTarget::TexSampled, BlitPostprocess::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
        {Ren::eBindTarget::TexSampled, BlitPostprocess::INPUT_TEX_SLOT, {*color_tex.ref, *args_->linear_sampler}},
        {Ren::eBindTarget::TexSampled, BlitPostprocess::BLOOM_TEX_SLOT, *bloom_tex.ref}};
    if (args_->tonemap_mode == 2 && lut_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, BlitPostprocess::LUT_TEX_SLOT, *lut_tex->ref);
    }

    Ren::SmallVector<Ren::RenderTarget, 2> render_targets = {
        {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
    if (output_tex2) {
        render_targets.emplace_back(output_tex2->ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store);
    }

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_postprocess_prog_[args_->tonemap_mode == 2][output_tex2 != nullptr],
                        {}, render_targets, rast_state, fg.rast_state(), bindings, &uniform_params,
                        sizeof(BlitPostprocess::Params), 0);
}
