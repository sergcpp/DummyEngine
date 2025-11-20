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
    const Ren::Image &exposure_tex = fg.AccessROImage(args_->exposure_tex);
    const Ren::Image &color_tex = fg.AccessROImage(args_->color_tex);
    const Ren::Image &bloom_tex = fg.AccessROImage(args_->bloom_tex);
    Ren::WeakImgRef output_tex = fg.AccessRWImageRef(args_->output_tex);
    const Ren::Image *lut_tex = nullptr;
    if (args_->lut_tex) {
        lut_tex = &fg.AccessROImage(args_->lut_tex);
    }
    Ren::WeakImgRef output_tex2;
    if (args_->output_tex2) {
        output_tex2 = fg.AccessRWImageRef(args_->output_tex2);
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
        {Ren::eBindTarget::TexSampled, BlitPostprocess::EXPOSURE_TEX_SLOT, exposure_tex},
        {Ren::eBindTarget::TexSampled, BlitPostprocess::INPUT_TEX_SLOT, {color_tex, *args_->linear_sampler}},
        {Ren::eBindTarget::TexSampled, BlitPostprocess::BLOOM_TEX_SLOT, bloom_tex}};
    if (args_->tonemap_mode == 2 && lut_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, BlitPostprocess::LUT_TEX_SLOT, *lut_tex);
    }

    Ren::SmallVector<Ren::RenderTarget, 2> render_targets = {
        {output_tex, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
    if (output_tex2) {
        render_targets.emplace_back(output_tex2, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store);
    }

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_postprocess_prog_[args_->tonemap_mode == 2][bool(output_tex2)], {},
                        render_targets, rast_state, fg.rast_state(), bindings, &uniform_params,
                        sizeof(BlitPostprocess::Params), 0);
}
