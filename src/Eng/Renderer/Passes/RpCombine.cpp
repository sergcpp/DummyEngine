#include "RpCombine.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_combine_interface.glsl"

void RpCombine::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetReadTexture(pass_data_->color_tex);
    RpAllocTex &blur_tex = builder.GetReadTexture(pass_data_->blur_tex);
    RpAllocTex &exposure_tex = builder.GetReadTexture(pass_data_->exposure_tex);
    RpAllocTex *output_tex = nullptr;
    if (pass_data_->output_tex) {
        output_tex = &builder.GetWriteTexture(pass_data_->output_tex);
    }

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    if (pass_data_->output_tex) {
        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];
    } else {
        rast_state.viewport[2] = view_state_->scr_res[0];
        rast_state.viewport[3] = view_state_->scr_res[1];
    }

    BlitCombine::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]), float(view_state_->act_res[1])};
    uniform_params.tex_size = Ren::Vec2f{float(view_state_->scr_res[0]), float(view_state_->scr_res[1])};
    uniform_params.tonemap = pass_data_->tonemap ? 1.0f : 0.0f;
    uniform_params.gamma = pass_data_->gamma;
    uniform_params.exposure = pass_data_->tonemap ? pass_data_->exposure : 1.0f;
    uniform_params.fade = pass_data_->fade;

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, BlitCombine::HDR_TEX_SLOT, *color_tex.ref},
                                     {Ren::eBindTarget::Tex2D, BlitCombine::BLURED_TEX_SLOT, *blur_tex.ref}};

    const Ren::WeakTex2DRef output = output_tex ? output_tex->ref : Ren::WeakTex2DRef(builder.ctx().backbuffer_ref());
    const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_combine_prog_, render_targets, {}, rast_state, builder.rast_state(),
                        bindings, &uniform_params, sizeof(BlitCombine::Params), 0);
}

void RpCombine::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex) {
    if (!initialized) {
        blit_combine_prog_ =
            sh.LoadProgram(ctx, "blit_combine2", "internal/blit_combine.vert.glsl", "internal/blit_combine.frag.glsl");
        assert(blit_combine_prog_->ready());

        initialized = true;
    }
}
