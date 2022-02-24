#include "RpSSRReproject.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_reproject_interface.glsl"

void RpSSRReproject::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf_name[],
                           const char depth_tex_name[], const char norm_tex_name[], Ren::WeakTex2DRef depth_history_tex,
                           Ren::WeakTex2DRef norm_history_tex, const char refl_tex_name[], const char raylen_tex_name[],
                           Ren::WeakTex2DRef refl_history_tex, const char velocity_tex_name[],
                           Ren::WeakTex2DRef variance_history_tex, Ren::WeakTex2DRef sample_count_history_tex,
                           const char tile_list_buf_name[], const char indir_args_name[], uint32_t indir_args_off,
                           const char out_reprojected_refl_tex_name[], const char out_avg_refl_tex_name[],
                           Ren::WeakTex2DRef out_variance_tex, Ren::WeakTex2DRef out_sample_count_tex) {
    view_state_ = view_state;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    depth_hist_tex_ =
        builder.ReadTexture(depth_history_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_hist_tex_ =
        builder.ReadTexture(norm_history_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    raylen_tex_ =
        builder.ReadTexture(raylen_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_hist_tex_ =
        builder.ReadTexture(refl_history_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    velocity_tex_ =
        builder.ReadTexture(velocity_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    variance_hist_tex_ = builder.ReadTexture(variance_history_tex, Ren::eResState::ShaderResource,
                                             Ren::eStageBits::ComputeShader, *this);
    sample_count_hist_tex_ = builder.ReadTexture(sample_count_history_tex, Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::ComputeShader, *this);
    tile_list_buf_ =
        builder.ReadBuffer(tile_list_buf_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
    indir_args_off_ = indir_args_off;

    { // Reprojected reflections texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_reprojected_tex_ =
            builder.WriteTexture(out_reprojected_refl_tex_name, params, Ren::eResState::UnorderedAccess,
                                 Ren::eStageBits::ComputeShader, *this);
    }
    { // 8x8 average reflections texture
        Ren::Tex2DParams params;
        params.w = (view_state->scr_res[0] + 7) / 8;
        params.h = (view_state->scr_res[1] + 7) / 8;
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_avg_refl_tex_ = builder.WriteTexture(out_avg_refl_tex_name, params, Ren::eResState::UnorderedAccess,
                                                 Ren::eStageBits::ComputeShader, *this);
    }

    out_variance_tex_ =
        builder.WriteTexture(out_variance_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
    out_sample_count_tex_ = builder.WriteTexture(out_sample_count_tex, Ren::eResState::UnorderedAccess,
                                                 Ren::eStageBits::ComputeShader, *this);
}

void RpSSRReproject::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_reproject_prog = sh.LoadProgram(ctx, "ssr_reproject", "internal/ssr_reproject.comp.glsl");
        assert(ssr_reproject_prog->ready());

        if (!pi_ssr_reproject_.Init(ctx.api_ctx(), std::move(ssr_reproject_prog), ctx.log())) {
            ctx.log()->Error("RpSSRReproject: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRReproject::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &depth_hist_tex = builder.GetReadTexture(depth_hist_tex_);
    RpAllocTex &norm_hist_tex = builder.GetReadTexture(norm_hist_tex_);
    RpAllocTex &relf_tex = builder.GetReadTexture(refl_tex_);
    RpAllocTex &raylen_tex = builder.GetReadTexture(raylen_tex_);
    RpAllocTex &refl_hist_tex = builder.GetReadTexture(refl_hist_tex_);
    RpAllocTex &velocity_tex = builder.GetReadTexture(velocity_tex_);
    RpAllocTex &variance_hist_tex = builder.GetReadTexture(variance_hist_tex_);
    RpAllocTex &sample_count_hist_tex = builder.GetReadTexture(sample_count_hist_tex_);
    RpAllocBuf &shared_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocBuf &tile_list_buf = builder.GetReadBuffer(tile_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);

    RpAllocTex &out_reprojected_tex = builder.GetWriteTexture(out_reprojected_tex_);
    RpAllocTex &out_avg_refl_tex = builder.GetWriteTexture(out_avg_refl_tex_);
    RpAllocTex &out_variance_tex = builder.GetWriteTexture(out_variance_tex_);
    RpAllocTex &out_sample_count_tex = builder.GetWriteTexture(out_sample_count_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRReproject::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::DEPTH_HIST_TEX_SLOT, *depth_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::REFL_TEX_SLOT, *relf_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::RAYLEN_TEX_SLOT, *raylen_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::REFL_HIST_TEX_SLOT, *refl_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::VELOCITY_TEX_SLOT, *velocity_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRReproject::SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *shared_data_buf.ref},
        {Ren::eBindTarget::SBuf, SSRReproject::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},

        {Ren::eBindTarget::Image, SSRReproject::OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
        {Ren::eBindTarget::Image, SSRReproject::OUT_AVG_REFL_IMG_SLOT, *out_avg_refl_tex.ref},
        {Ren::eBindTarget::Image, SSRReproject::OUT_VERIANCE_IMG_SLOT, *out_variance_tex.ref},
        {Ren::eBindTarget::Image, SSRReproject::OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

    SSRReproject::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{GLOSSY_THRESHOLD, MIRROR_THRESHOLD};

    Ren::DispatchComputeIndirect(pi_ssr_reproject_, *indir_args_buf.ref, indir_args_off_, bindings, COUNT_OF(bindings),
                                 &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                 builder.ctx().log());
}
