#include "RpTAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_taa_interface.glsl"

void RpTAA::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(pass_data_->shared_data);

    RpAllocTex &clean_tex = builder.GetReadTexture(pass_data_->clean_tex);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &velocity_tex = builder.GetReadTexture(pass_data_->velocity_tex);
    RpAllocTex &history_tex = builder.GetReadTexture(pass_data_->history_tex);
    RpAllocTex &output_tex = builder.GetWriteTexture(pass_data_->output_tex);

    LazyInit(builder.ctx(), builder.sh(), depth_tex, velocity_tex, history_tex, output_tex);

    Ren::RastState rast_state;
    rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

    rast_state.viewport[2] = view_state_->act_res[0];
    rast_state.viewport[3] = view_state_->act_res[1];

    { // Blit taa
        // exposure from previous frame
        float exposure = reduced_average_ > std::numeric_limits<float>::epsilon() ? (1.0f / reduced_average_) : 1.0f;
        exposure = std::min(exposure, max_exposure_);

        const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, TempAA::CURR_TEX_SLOT, *clean_tex.ref},
                                         {Ren::eBindTarget::Tex2D, TempAA::HIST_TEX_SLOT, *history_tex.ref},
                                         {Ren::eBindTarget::Tex2D, TempAA::DEPTH_TEX_SLOT, *depth_tex.ref},
                                         {Ren::eBindTarget::Tex2D, TempAA::VELOCITY_TEX_SLOT, *velocity_tex.ref}};

        TempAA::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_->act_res[0], view_state_->act_res[1]};
        uniform_params.tex_size = Ren::Vec2f{float(view_state_->act_res[0]), float(view_state_->act_res[1])};
        uniform_params.exposure = exposure;

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_taa_prog_, resolve_fb_, render_pass_, rast_state,
                            builder.rast_state(), bindings, 4, &uniform_params, sizeof(TempAA::Params), 0);
    }
}

void RpTAA::LazyInit(Ren::Context &ctx, ShaderLoader &sh, RpAllocTex &depth_tex, RpAllocTex &velocity_tex,
                     RpAllocTex &history_tex, RpAllocTex &output_tex) {
    if (!initialized) {
        blit_taa_prog_ = sh.LoadProgram(ctx, "blit_taa_prog", "internal/blit_taa.vert.glsl",
                                        "internal/blit_taa.frag.glsl@USE_CLIPPING;USE_TONEMAP");
        assert(blit_taa_prog_->ready());

        initialized = true;
    }

    const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

    if (!render_pass_.Setup(ctx.api_ctx(), render_targets, 1, {}, ctx.log())) {
        ctx.log()->Error("RpCombine: render_pass_ init failed!");
    }

    if (!resolve_fb_.Setup(ctx.api_ctx(), render_pass_, output_tex.desc.w, output_tex.desc.h, {}, {}, render_targets,
                           1)) {
        ctx.log()->Error("RpTAA: resolve_fb_ init failed!");
    }
}
