#include "RpCombine.h"

#include <cassert>

#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpCombine::Setup(RpBuilder &builder, const ViewState *view_state, float gamma,
                      float exposure, float fade, bool tonemap, Ren::TexHandle color_tex,
                      Ren::TexHandle blur_tex, Ren::TexHandle output_tex) {
    view_state_ = view_state;
    gamma_ = gamma;
    exposure_ = exposure;
    fade_ = fade;
    tonemap_ = tonemap;

    color_tex_ = color_tex;
    blur_tex_ = blur_tex;
    output_tex_ = output_tex;

    // input_[0] = builder.ReadBuffer(in_shared_data_buf);
    input_count_ = 0;

    // output_[0] = builder.WriteBuffer(input_[0], *this);
    output_count_ = 0;
}

void RpCombine::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    if (output_tex_) {
        rast_state.viewport[2] = view_state_->act_res[0];
        rast_state.viewport[3] = view_state_->act_res[1];
    } else {
        rast_state.viewport[2] = view_state_->scr_res[0];
        rast_state.viewport[3] = view_state_->scr_res[1];
    }

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0]),
                       float(view_state_->act_res[1])}},
        {12, tonemap_ ? 1.0f : 0.0f},
        {13, Ren::Vec2f{float(view_state_->scr_res[0]), float(view_state_->scr_res[1])}},
        {14, gamma_},
        {15, tonemap_ ? exposure_ : 1.0f},
        {16, fade_}};

    const PrimDraw::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, color_tex_},
        {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT, blur_tex_}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0},
                        blit_combine_prog_.get(), bindings, 2, uniforms, 6);
}

void RpCombine::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        blit_combine_prog_ =
            sh.LoadProgram(ctx, "blit_combine", "internal/blit.vert.glsl",
                           "internal/blit_combine.frag.glsl");
        assert(blit_combine_prog_->ready());

        initialized = true;
    }

    if (!output_fb_.Setup(&output_tex_, 1, {}, {}, false)) {
        ctx.log()->Error("RpCombine: output_fb_ init failed!");
    }
}
