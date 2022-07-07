#include "RpSSRTrace.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_ssr_interface.glsl"

void RpSSRTrace::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &normal_tex = builder.GetReadTexture(pass_data_->normal_tex);
    RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(pass_data_->depth_down_2x_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

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

        const Ren::Binding bindings[] = {
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

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, {}, ctx.log())) {
        ctx.log()->Error("RpSSRTrace: render_pass_ init failed!");
    }

    if (!output_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, {}, {}, render_targets,
                          ctx.log())) {
        ctx.log()->Error("RpSSRTrace: output_fb_ init failed!");
    }
}
