#include "RpSSRDilate.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Scene/ProbeStorage.h"
#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssr_dilate_interface.glsl"

void RpSSRDilate::Setup(RpBuilder &builder, const ViewState *view_state, const char ssr_tex_name[],
                        const char output_tex_name[]) {
    view_state_ = view_state;

    ssr_tex_ =
        builder.ReadTexture(ssr_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Auxilary texture for reflections (rg - uvs, b - influence)
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0] / 2;
        params.h = view_state->scr_res[1] / 2;
        params.format = Ren::eTexFormat::RawRGB10_A2;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

void RpSSRDilate::Execute(RpBuilder &builder) {
    RpAllocTex &ssr_tex = builder.GetReadTexture(ssr_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);
    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.depth.test_enabled = false;
    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->scr_res[0] / 2;
    rast_state.viewport[3] = view_state_->scr_res[1] / 2;

    { // dilate ssr buffer
        Ren::Program *dilate_prog = blit_ssr_dilate_prog_.get();

        const PrimDraw::Binding bindings[] = {{Ren::eBindTarget::Tex2D, SSRDilate::SSR_TEX_SLOT, *ssr_tex.ref}};

        SSRDilate::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_->act_res[0] / 2, view_state_->act_res[1] / 2};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_dilate_prog_, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params,
                            sizeof(SSRDilate::Params), 0);
    }
}

void RpSSRDilate::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_dilate_prog_ = sh.LoadProgram(ctx, "blit_ssr_dilate", "internal/blit_ssr_dilate.vert.glsl",
                                               "internal/blit_ssr_dilate.frag.glsl");
        assert(blit_ssr_dilate_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSRDilate: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpSSRDilate: output_fb_ init failed!");
    }
}
