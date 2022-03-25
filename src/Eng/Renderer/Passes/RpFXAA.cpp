#include "RpFXAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

void RpFXAA::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf[],
                   const char color_tex[], const char output_tex_name[]) {
    view_state_ = view_state;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    color_tex_ = builder.ReadTexture(color_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    if (output_tex_name) {
        output_tex_ = builder.WriteTexture(output_tex_name, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    } else {
        output_tex_ = {};
    }
}

void RpFXAA::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &color_tex = builder.GetReadTexture(color_tex_);
    RpAllocTex *output_tex = nullptr;
    if (output_tex_) {
        output_tex = &builder.GetWriteTexture(output_tex_);
    }

    LazyInit(builder.ctx(), builder.sh(), &color_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    rast_state.ApplyChanged(builder.rast_state());
    builder.rast_state() = rast_state;

    Ren::Program *blit_prog = blit_fxaa_prog_.get();

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, REN_BASE0_TEX_SLOT, *color_tex.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

    const PrimDraw::Uniform uniforms[] = {
        {0, Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f}},
        {12, Ren::Vec2f{1.0f / float(view_state_->scr_res[0]), 1.0f / float(view_state_->scr_res[1])}}};

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, {&output_fb_, 0}, blit_prog, bindings, 2, uniforms, 2);
}

void RpFXAA::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex *output_tex) {
    if (!initialized) {
        blit_fxaa_prog_ =
            sh.LoadProgram(ctx, "blit_fxaa_prog", "internal/blit.vert.glsl", "internal/blit_fxaa.frag.glsl");
        assert(blit_fxaa_prog_->ready());

        initialized = true;
    }

    const Ren::WeakTex2DRef output = output_tex ? output_tex->ref : Ren::WeakTex2DRef{};
    if (!output_fb_.Setup(ctx.api_ctx(), {}, view_state_->act_res[0], view_state_->act_res[1], {}, {}, &output, 1,
                          false)) {
        ctx.log()->Error("RpFXAA: output_fb_ init failed!");
    }
}
