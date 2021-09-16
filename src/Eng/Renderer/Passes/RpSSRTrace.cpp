#include "RpSSRTrace.h"

#include <Ren/Context.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Scene/ProbeStorage.h"
#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssr_interface.glsl"

void RpSSRTrace::Setup(RpBuilder &builder, const ViewState *view_state, Ren::Tex2DRef brdf_lut,
                       const char shared_data_buf[], const char normal_tex[], const char depth_down_2x[],
                       const char output_tex_name[]) {
    view_state_ = view_state;
    brdf_lut_ = brdf_lut;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);
    normal_tex_ =
        builder.ReadTexture(normal_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    depth_down_2x_tex_ =
        builder.ReadTexture(depth_down_2x, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

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

void RpSSRTrace::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &normal_tex = builder.GetReadTexture(normal_tex_);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(depth_down_2x_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

    LazyInit(builder.ctx(), builder.sh(), output_tex);

    Ren::RastState rast_state;
    rast_state.depth.test_enabled = false;
    rast_state.depth.write_enabled = false;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->scr_res[0] / 2;
    rast_state.viewport[3] = view_state_->scr_res[1] / 2;

    const Ren::eBindTarget clean_buf_bind_target =
        view_state_->is_multisampled ? Ren::eBindTarget::Tex2DMs : Ren::eBindTarget::Tex2D;

    { // screen space tracing
        const Ren::ProgramRef ssr_program = view_state_->is_multisampled ? blit_ssr_ms_prog_ : blit_ssr_prog_;

        const PrimDraw::Binding bindings[] = {
            {Ren::eBindTarget::Tex2D, SSRTrace::DEPTH_TEX_SLOT, *depth_down_2x_tex.ref},
            {clean_buf_bind_target, SSRTrace::NORM_TEX_SLOT, *normal_tex.ref},
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref}};

        SSRTrace::Params uniform_params;
        uniform_params.transform =
            Ren::Vec4f{0.0f, 0.0f, float(view_state_->act_res[0] / 2), float(view_state_->act_res[1] / 2)};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, ssr_program, output_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, COUNT_OF(bindings), &uniform_params,
                            sizeof(SSRTrace::Params), 0);
    }
}

void RpSSRTrace::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_ssr_prog_ = sh.LoadProgram(ctx, "blit_ssr", "internal/blit_ssr.vert.glsl", "internal/blit_ssr.frag.glsl");
        assert(blit_ssr_prog_->ready());
        blit_ssr_ms_prog_ =
            sh.LoadProgram(ctx, "blit_ssr_ms", "internal/blit_ssr.vert.glsl", "internal/blit_ssr.frag.glsl@MSAA_4");
        assert(blit_ssr_ms_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpSSRTrace: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, render_targets, 1, {},
                          {})) {
        ctx.log()->Error("RpSSRTrace: output_fb_ init failed!");
    }
}
