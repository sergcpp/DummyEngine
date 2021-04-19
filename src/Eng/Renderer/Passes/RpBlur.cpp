#include "RpBlur.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../../Utils/ShaderLoader.h"

void RpBlur::Setup(RpBuilder &builder, const ViewState *view_state,
                   Ren::TexHandle down_buf_4x, const char blur_res_tex_name[]) {
    view_state_ = view_state;
    down_buf_4x_ = down_buf_4x;

    { // Auxilary textures for bloom effect
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 4;
        params.h = view_state->scr_res[1] / 4;
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        blur_temp_4x_ = builder.WriteTexture("Blur temp", params, *this);
        output_tex_ = builder.WriteTexture(blur_res_tex_name, params, *this);
    }
}

void RpBlur::Execute(RpBuilder &builder) {
    RpAllocTex &blur_temp_4x = builder.GetWriteTexture(blur_temp_4x_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), blur_temp_4x, output_tex);

    Ren::RastState rast_state;
    rast_state.cull_face.enabled = true;

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    rast_state.Apply();
    Ren::RastState applied_state = rast_state;

    { // horizontal
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           down_buf_4x_};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(applied_state.viewport[2]),
                           float(applied_state.viewport[3])}},
            {1, 0.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[0].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }

    { // vertical
        const PrimDraw::Binding binding = {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT,
                                           blur_temp_4x.ref->handle()};

        const PrimDraw::Uniform uniforms[] = {
            {0, Ren::Vec4f{0.0f, 0.0f, float(applied_state.viewport[2]),
                           float(applied_state.viewport[3])}},
            {1, 1.0f}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {blur_fb_[1].id(), 0},
                            blit_gauss_prog_.get(), &binding, 1, uniforms, 2);
    }
}

void RpBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &blur_temp_4x,
                      RpAllocTex &output_tex) {
    if (!initialized) {
        blit_gauss_prog_ = sh.LoadProgram(ctx, "blit_gauss", "internal/blit.vert.glsl",
                                          "internal/blit_gauss.frag.glsl");
        assert(blit_gauss_prog_->ready());

        initialized = true;
    }

    if (!blur_fb_[0].Setup(blur_temp_4x.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpBlur: blur_fb_[0] init failed!");
    }

    if (!blur_fb_[1].Setup(output_tex.ref->handle(), {}, {}, false)) {
        ctx.log()->Error("RpBlur: blur_fb_[1] init failed!");
    }
}
