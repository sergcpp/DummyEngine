#include "RpSSRResolveTemporal.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_resolve_temporal_interface.glsl"

void RpSSRResolveTemporal::Setup(RpBuilder &builder, const ViewState *view_state, const char shared_data_buf_name[],
                                 const char depth_tex_name[], const char norm_tex_name[], const char rough_tex_name[],
                                 Ren::WeakTex2DRef norm_hist_tex, Ren::WeakTex2DRef rough_hist_tex,
                                 const char velocity_tex_name[], const char refl_tex_name[],
                                 Ren::WeakTex2DRef refl_hist_tex, const char ray_len_tex_name[],
                                 const char tile_metadata_mask_name[], const char temp_variance_mask_name[],
                                 const char denoised_img_name[]) {
    view_state_ = view_state;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    rough_tex_ =
        builder.ReadTexture(rough_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_hist_tex_ =
        builder.ReadTexture(norm_hist_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    rough_hist_tex_ =
        builder.ReadTexture(rough_hist_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    velocity_tex_ =
        builder.ReadTexture(velocity_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_hist_tex_ =
        builder.ReadTexture(refl_hist_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    ray_len_tex_ =
        builder.ReadTexture(ray_len_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    tile_metadata_mask_buf_ = builder.ReadBuffer(tile_metadata_mask_name, Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::ComputeShader, *this);
    temp_variance_mask_buf_ = builder.ReadBuffer(temp_variance_mask_name, Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::ComputeShader, *this);
    { // reflection color texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_denoised_tex_ = builder.WriteTexture(denoised_img_name, params, Ren::eResState::UnorderedAccess,
                                                 Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRResolveTemporal::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_resolve_spacial_prog =
            sh.LoadProgram(ctx, "ssr_resolve_temporal", "internal/ssr_resolve_temporal.comp.glsl");
        assert(ssr_resolve_spacial_prog->ready());

        if (!pi_ssr_resolve_temporal_.Init(ctx.api_ctx(), std::move(ssr_resolve_spacial_prog), ctx.log())) {
            ctx.log()->Error("RpSSRResolveTemporal: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRResolveTemporal::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &rough_tex = builder.GetReadTexture(rough_tex_);
    RpAllocTex &norm_hist_tex = builder.GetReadTexture(norm_hist_tex_);
    RpAllocTex &rough_hist_tex = builder.GetReadTexture(rough_hist_tex_);
    RpAllocTex &velocity_tex = builder.GetReadTexture(velocity_tex_);
    RpAllocTex &refl_tex = builder.GetReadTexture(refl_tex_);
    RpAllocTex &refl_hist_tex = builder.GetReadTexture(refl_hist_tex_);
    RpAllocTex &ray_len_tex = builder.GetReadTexture(ray_len_tex_);
    RpAllocBuf &tile_metadata_mask_buf = builder.GetReadBuffer(tile_metadata_mask_buf_);
    RpAllocBuf &temp_variance_mask_buf = builder.GetReadBuffer(temp_variance_mask_buf_);

    RpAllocTex &out_denoised_tex = builder.GetWriteTexture(out_denoised_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::ROUGH_TEX_SLOT, *rough_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::ROUGH_HIST_TEX_SLOT, *rough_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::VELOCITY_TEX_SLOT, *velocity_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REFL_TEX_SLOT, *refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REFL_HIST_TEX_SLOT, *refl_hist_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::RAY_LEN_TEX_SLOT, *ray_len_tex.ref},
        {Ren::eBindTarget::SBuf, SSRResolveTemporal::TILE_METADATA_MASK_SLOT, *tile_metadata_mask_buf.ref},
        {Ren::eBindTarget::SBuf, SSRResolveTemporal::TEMP_VARIANCE_MASK_SLOT, *temp_variance_mask_buf.ref},
        {Ren::eBindTarget::Image, SSRResolveTemporal::OUT_DENOISED_IMG_SLOT, *out_denoised_tex.ref}};

    const auto grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRResolveTemporal::LOCAL_GROUP_SIZE_X - 1u) / SSRResolveTemporal::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRResolveTemporal::LOCAL_GROUP_SIZE_Y - 1u) / SSRResolveTemporal::LOCAL_GROUP_SIZE_Y,
        1u};

    SSRResolveTemporal::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{GLOSSY_THRESHOLD, MIRROR_THRESHOLD};

    Ren::DispatchCompute(pi_ssr_resolve_temporal_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}
