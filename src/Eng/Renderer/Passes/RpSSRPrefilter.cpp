#include "RpSSRPrefilter.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_prefilter_interface.glsl"

void RpSSRPrefilter::Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[],
                           const char norm_tex_name[], const char avg_refl_tex_name[], const char refl_tex_name[],
                           Ren::WeakTex2DRef variance_tex, Ren::WeakTex2DRef sample_count_tex,
                           const char tile_list_buf_name[], const char indir_args_name[], uint32_t indir_args_off,
                           const char out_refl_tex_name[], Ren::WeakTex2DRef out_variance_tex) {
    view_state_ = view_state;

    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    avg_refl_tex_ =
        builder.ReadTexture(avg_refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    variance_tex_ =
        builder.ReadTexture(variance_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    sample_count_tex_ =
        builder.ReadTexture(sample_count_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    tile_list_buf_ =
        builder.ReadBuffer(tile_list_buf_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
    indir_args_off_ = indir_args_off;

    { // Reflection color texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_refl_tex_ = builder.WriteTexture(out_refl_tex_name, params, Ren::eResState::UnorderedAccess,
                                             Ren::eStageBits::ComputeShader, *this);
    }

    out_variance_tex_ =
        builder.WriteTexture(out_variance_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
}

void RpSSRPrefilter::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_prefilter_prog = sh.LoadProgram(ctx, "ssr_prefilter", "internal/ssr_prefilter.comp.glsl");
        assert(ssr_prefilter_prog->ready());

        if (!pi_ssr_prefilter_.Init(ctx.api_ctx(), std::move(ssr_prefilter_prog), ctx.log())) {
            ctx.log()->Error("RpSSRPrefilter: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRPrefilter::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &avg_refl_tex = builder.GetReadTexture(avg_refl_tex_);
    RpAllocTex &refl_tex = builder.GetReadTexture(refl_tex_);
    RpAllocTex &variance_tex = builder.GetReadTexture(variance_tex_);
    RpAllocTex &sample_count_tex = builder.GetReadTexture(sample_count_tex_);
    RpAllocBuf &tile_list_buf = builder.GetReadBuffer(tile_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(out_refl_tex_);
    RpAllocTex &out_variance_tex = builder.GetWriteTexture(out_variance_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRPrefilter::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRPrefilter::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRPrefilter::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRPrefilter::REFL_TEX_SLOT, *refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRPrefilter::VARIANCE_TEX_SLOT, *variance_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRPrefilter::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
        {Ren::eBindTarget::SBuf, SSRPrefilter::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},

        {Ren::eBindTarget::Image, SSRPrefilter::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image, SSRPrefilter::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

    const auto grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRPrefilter::LOCAL_GROUP_SIZE_X - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRPrefilter::LOCAL_GROUP_SIZE_Y - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_Y, 1u};

    SSRPrefilter::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{GLOSSY_THRESHOLD, MIRROR_THRESHOLD};

    Ren::DispatchComputeIndirect(pi_ssr_prefilter_, *indir_args_buf.ref, indir_args_off_, bindings, COUNT_OF(bindings),
                                 &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                 builder.ctx().log());
}
