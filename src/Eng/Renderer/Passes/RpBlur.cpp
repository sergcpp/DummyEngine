#include "RpBlur.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_gauss_interface.glsl"

void RpBlur::Setup(RpBuilder &builder, const ViewState *view_state, bool vertical, Ren::WeakTex2DRef input_tex,
                   const char output_tex_name[]) {
    vertical_ = vertical;
    view_state_ = view_state;

    input_tex_ = builder.ReadTexture(input_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Auxilary textures for bloom effect
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 4;
        params.h = view_state->scr_res[1] / 4;
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

void RpBlur::Setup(RpBuilder &builder, const ViewState *view_state, bool vertical, const char input_tex_name[],
                   const char output_tex_name[]) {
    vertical_ = vertical;
    view_state_ = view_state;

    input_tex_ =
        builder.ReadTexture(input_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Auxilary textures for bloom effect
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 4;
        params.h = view_state->scr_res[1] / 4;
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

void RpBlur::Execute(RpBuilder &builder) {
    RpAllocTex &intput_tex = builder.GetReadTexture(input_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0] / 4;
    rast_state.viewport[3] = view_state_->act_res[1] / 4;

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, Gauss::SRC_TEX_SLOT, *intput_tex.ref}};

    Gauss::Params uniform_params;
    uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(rast_state.viewport[2]), float(rast_state.viewport[3])};
    uniform_params.vertical[0] = vertical_ ? 1.0f : 0.0f;

    prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_gauss_prog_, output_fb_, render_pass_, rast_state,
                        builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params, sizeof(Gauss::Params), 0);
}

void RpBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_gauss_prog_ =
            sh.LoadProgram(ctx, "blit_gauss2", "internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
        assert(blit_gauss_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSRDilate: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, {}, {}, render_targets,
                          1)) {
        ctx.log()->Error("RpSSRDilate: output_fb_ init failed!");
    }
}
