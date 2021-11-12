#include "RpSSAO.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssao_interface.glsl"

void RpSSAO::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakTex2DRef rand2d_dirs_4x4_tex,
                   const char depth_down_2x[], const char output_tex[]) {
    view_state_ = view_state;

    depth_down_2x_tex_ =
        builder.ReadTexture(depth_down_2x, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    rand_tex_ = builder.ReadTexture(rand2d_dirs_4x4_tex, Ren::eResState::ShaderResource,
                                    Ren::eStageBits::FragmentShader, *this);

    { // Allocate output texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 2;
        params.h = view_state->scr_res[1] / 2;
        params.format = Ren::eTexFormat::RawR8;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

void RpSSAO::Execute(RpBuilder &builder) {
    RpAllocTex &down_depth_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &rand_tex = builder.GetReadTexture(rand_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0] / 2;
    rast_state.viewport[3] = view_state_->act_res[1] / 2;

    { // prepare ao buffer
        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, SSAO::DEPTH_TEX_SLOT, *down_depth_2x_tex.ref},
                                         {Ren::eBindTarget::Tex2D, SSAO::RAND_TEX_SLOT, *rand_tex.ref}};

        SSAO::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_->act_res[0] / 2, view_state_->act_res[1] / 2};
        uniform_params.resolution = Ren::Vec2f{float(view_state_->act_res[0]), float(view_state_->act_res[1])};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ao_prog_, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params, sizeof(SSAO::Params),
                            0);
    }
}

void RpSSAO::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ao_prog_ = sh.LoadProgram(ctx, "blit_ao", "internal/blit_ssao.vert.glsl", "internal/blit_ssao.frag.glsl");
        assert(blit_ao_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSAO: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpSSAO: output_fb_ init failed!");
    }
}
