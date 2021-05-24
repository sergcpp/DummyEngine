#include "RpCombine.h"

#include <cassert>

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpCombine::Setup(RpBuilder &builder, const ViewState *view_state, const float gamma,
                      const float exposure, const float fade, bool const tonemap,
                      const char color_tex_name[], const char blur_tex_name[],
                      const char output_tex_name[]) {
    view_state_ = view_state;
    gamma_ = gamma;
    exposure_ = exposure;
    fade_ = fade;
    tonemap_ = tonemap;

    color_tex_ = builder.ReadTexture(color_tex_name, *this);

    if (blur_tex_name) {
        blur_tex_ = builder.ReadTexture(blur_tex_name, *this);
    } else {
        blur_tex_ = {};
    }

    if (output_tex_name) {
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRGB888;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, *this);
    } else {
        output_tex_ = {};
    }
}

void RpCombine::Execute(RpBuilder &builder) {
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex *blur_tex = nullptr;
    if (blur_tex_) {
        blur_tex = &builder.GetReadTexture(blur_tex_);
    }
    RpAllocTex *output_tex = nullptr;
    if (output_tex_) {
        output_tex = &builder.GetWriteTexture(output_tex_);
    }

    LazyInit(builder.ctx(), builder.sh(), output_tex);

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
        {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, color_tex.ref->handle()},
        {Ren::eBindTarget::Tex2D, REN_BASE1_TEX_SLOT,
         blur_tex ? blur_tex->ref->handle() : dummy_black_->handle()}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {output_fb_.id(), 0},
                        blit_combine_prog_.get(), bindings, 2, uniforms, 6);
}

void RpCombine::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex) {
    if (!initialized) {
        blit_combine_prog_ =
            sh.LoadProgram(ctx, "blit_combine", "internal/blit.vert.glsl",
                           "internal/blit_combine.frag.glsl");
        assert(blit_combine_prog_->ready());

        static const uint8_t black[] = {0, 0, 0, 0};

        Ren::Tex2DParams p;
        p.w = p.h = 1;
        p.format = Ren::eTexFormat::RawRGBA8888;
        p.sampling.repeat = Ren::eTexRepeat::ClampToEdge;

        Ren::eTexLoadStatus status;
        dummy_black_ = ctx.LoadTexture2D("dummy_black", black, sizeof(black), p, &status);
        assert(status == Ren::eTexLoadStatus::CreatedFromData ||
               status == Ren::eTexLoadStatus::Found);

        initialized = true;
    }

    Ren::TexHandle output = output_tex ? output_tex->ref->handle() : Ren::TexHandle{};
    if (!output_fb_.Setup(&output, 1, {}, {}, false)) {
        ctx.log()->Error("RpCombine: output_fb_ init failed!");
    }
}
