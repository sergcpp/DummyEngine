#include "RpCombine.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/blit_combine_interface.h"

void Eng::RpCombine::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocTex &color_tex = builder.GetReadTexture(pass_data_->color_tex);
    RpAllocTex &blur_tex = builder.GetReadTexture(pass_data_->blur_tex);
    RpAllocTex &exposure_tex = builder.GetReadTexture(pass_data_->exposure_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);
    RpAllocTex *output_tex2 = nullptr;
    if (pass_data_->output_tex2) {
        output_tex2 = &builder.GetWriteTexture(pass_data_->output_tex2);
    }

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    BlitCombine::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
    uniform_params.tex_size = Ren::Vec2f{float(view_state_->scr_res[0]), float(view_state_->scr_res[1])};
    uniform_params.tonemap_mode = float(pass_data_->tonemap_mode);
    uniform_params.inv_gamma = pass_data_->inv_gamma;
    uniform_params.exposure = pass_data_->exposure;
    uniform_params.fade = pass_data_->fade;

    Ren::SmallVector<Ren::Binding, 8> bindings = {
        {Ren::eBindTarget::Tex2DSampled, BlitCombine::HDR_TEX_SLOT, *color_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, BlitCombine::BLURED_TEX_SLOT, *blur_tex.ref}};
    if (pass_data_->tonemap_mode == 2) {
        bindings.emplace_back(Ren::eBindTarget::Tex3DSampled, BlitCombine::LUT_TEX_SLOT, *pass_data_->lut_tex);
    }

    Ren::SmallVector<Ren::RenderTarget, 2> render_targets = {
        {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
    if (output_tex2) {
        render_targets.emplace_back(output_tex2->ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store);
    }

    prim_draw_.DrawPrim(
        PrimDraw::ePrim::Quad,
        blit_combine_prog_[pass_data_->compressed][pass_data_->tonemap_mode == 2][output_tex2 != nullptr],
        render_targets, {}, rast_state, builder.rast_state(), bindings, &uniform_params, sizeof(BlitCombine::Params),
        0);
}

void Eng::RpCombine::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        blit_combine_prog_[0][0][0] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl");
        assert(blit_combine_prog_[0][0][0]->ready());
        blit_combine_prog_[0][0][1] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl@TWO_TARGETS");
        assert(blit_combine_prog_[0][0][1]->ready());
        blit_combine_prog_[0][1][0] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl@LUT");
        assert(blit_combine_prog_[0][1][0]->ready());
        blit_combine_prog_[0][1][1] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl@LUT;TWO_TARGETS");
        assert(blit_combine_prog_[0][1][1]->ready());

        blit_combine_prog_[1][0][0] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl@COMPRESSED");
        assert(blit_combine_prog_[1][0][0]->ready());
        blit_combine_prog_[1][0][1] = sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl",
                                                     "internal/blit_combine.frag.glsl@COMPRESSED;TWO_TARGETS");
        assert(blit_combine_prog_[1][0][1]->ready());
        blit_combine_prog_[1][1][0] =
            sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl@COMPRESSED;LUT");
        assert(blit_combine_prog_[1][1][0]->ready());
        blit_combine_prog_[1][1][1] = sh.LoadProgram(ctx, "internal/blit_combine.vert.glsl",
                                                     "internal/blit_combine.frag.glsl@COMPRESSED;LUT;TWO_TARGETS");
        assert(blit_combine_prog_[1][1][1]->ready());

        initialized = true;
    }
}
