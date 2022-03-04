#include "RpSSRResolveTemporal.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_resolve_temporal_interface.glsl"

void RpSSRResolveTemporal::Setup(RpBuilder &builder, const ViewState *view_state, const float glossy_thres,
                                 const float mirror_thres, const char shared_data_buf_name[],
                                 const char norm_tex_name[], const char avg_refl_tex_name[], const char refl_tex_name[],
                                 const char reproj_refl_tex_name[], Ren::WeakTex2DRef variance_tex,
                                 Ren::WeakTex2DRef sample_count_tex, const char tile_list_buf_name[],
                                 const char indir_args_name[], uint32_t indir_args_off, Ren::WeakTex2DRef out_refl_tex,
                                 Ren::WeakTex2DRef out_variance_tex) {
    view_state_ = view_state;
    glossy_thres_ = glossy_thres;
    mirror_thres_ = mirror_thres;

    shared_data_buf_ =
        builder.ReadBuffer(shared_data_buf_name, Ren::eResState::UniformBuffer, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    avg_refl_tex_ =
        builder.ReadTexture(avg_refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    reproj_refl_tex_ = builder.ReadTexture(reproj_refl_tex_name, Ren::eResState::ShaderResource,
                                           Ren::eStageBits::ComputeShader, *this);
    variance_tex_ =
        builder.ReadTexture(variance_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    sample_count_tex_ =
        builder.ReadTexture(sample_count_tex, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    tile_list_buf_ =
        builder.ReadBuffer(tile_list_buf_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    indir_args_buf_ =
        builder.ReadBuffer(indir_args_name, Ren::eResState::IndirectArgument, Ren::eStageBits::DrawIndirect, *this);
    indir_args_off_ = indir_args_off;

    out_refl_tex_ =
        builder.WriteTexture(out_refl_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
    out_variance_tex_ =
        builder.WriteTexture(out_variance_tex, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);
}

void RpSSRResolveTemporal::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef prog = sh.LoadProgram(ctx, "ssr_resolve_temporal", "internal/ssr_resolve_temporal.comp.glsl");
        assert(prog->ready());

        if (!pi_ssr_resolve_temporal_.Init(ctx.api_ctx(), std::move(prog), ctx.log())) {
            ctx.log()->Error("RpSSRResolveTemporal: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRResolveTemporal::Execute(RpBuilder &builder) {
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(shared_data_buf_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &avg_refl_tex = builder.GetReadTexture(avg_refl_tex_);
    RpAllocTex &refl_tex = builder.GetReadTexture(refl_tex_);
    RpAllocTex &reproj_refl_tex = builder.GetReadTexture(reproj_refl_tex_);
    RpAllocTex &variance_tex = builder.GetReadTexture(variance_tex_);
    RpAllocTex &sample_count_tex = builder.GetReadTexture(sample_count_tex_);
    RpAllocBuf &tile_list_buf = builder.GetReadBuffer(tile_list_buf_);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(indir_args_buf_);

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(out_refl_tex_);
    RpAllocTex &out_variance_tex = builder.GetWriteTexture(out_variance_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REFL_TEX_SLOT, *refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REPROJ_REFL_TEX_SLOT, *reproj_refl_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::VARIANCE_TEX_SLOT, *variance_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveTemporal::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
        {Ren::eBindTarget::SBuf, SSRResolveTemporal::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
        {Ren::eBindTarget::Image, SSRResolveTemporal::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image, SSRResolveTemporal::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

    SSRResolveTemporal::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{glossy_thres_, mirror_thres_};

    Ren::DispatchComputeIndirect(pi_ssr_resolve_temporal_, *indir_args_buf.ref, indir_args_off_, bindings,
                                 COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
}
