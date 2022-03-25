#include "RpTAA.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/blit_taa_interface.glsl"

void RpTAA::Setup(RpBuilder &builder, const ViewState *view_state, Ren::WeakTex2DRef history_tex, float reduced_average,
                  float max_exposure, const char shared_data_buf[], const char color_tex[], const char depth_tex[],
                  const char velocity_tex[], const char output_tex_name[]) {
    view_state_ = view_state;

    reduced_average_ = reduced_average;
    max_exposure_ = max_exposure;

    shared_data_buf_ = builder.ReadBuffer(shared_data_buf, Ren::eResState::UniformBuffer,
                                          Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader, *this);

    clean_tex_ = builder.ReadTexture(color_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    depth_tex_ = builder.ReadTexture(depth_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    velocity_tex_ =
        builder.ReadTexture(velocity_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);
    history_tex_ =
        builder.ReadTexture(history_tex, Ren::eResState::ShaderResource, Ren::eStageBits::FragmentShader, *this);

    { // Texture that holds resolved color
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex_ = builder.WriteTexture(output_tex_name, params, Ren::eResState::RenderTarget,
                                           Ren::eStageBits::ColorAttachment, *this);
    }
}

void RpTAA::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(shared_data_buf_);

    RpAllocTex &clean_tex = builder.GetReadTexture(clean_tex_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &velocity_tex = builder.GetReadTexture(velocity_tex_);
    RpAllocTex &history_tex = builder.GetReadTexture(history_tex_);
    RpAllocTex &output_tex = builder.GetWriteTexture(output_tex_);

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
