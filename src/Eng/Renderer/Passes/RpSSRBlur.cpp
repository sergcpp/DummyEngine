#include "RpSSRBlur.h"
#if 0
#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_blur_interface.glsl"

void RpSSRBlur::Setup(RpBuilder &builder, const ViewState *view_state, const float glossy_thres,
                      const float mirror_thres, const char rough_tex_name[], const char refl_tex_name[],
                      const char tile_metadata_mask_name[], const char out_denoised_img_name[]) {
    view_state_ = view_state;
    glossy_thres_ = glossy_thres;
    mirror_thres_ = mirror_thres;

    rough_tex_ =
        builder.ReadTexture(rough_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    refl_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    tile_metadata_mask_buf_ = builder.ReadBuffer(tile_metadata_mask_name, Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::ComputeShader, *this);

    { // Reflection color texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_denoised_tex_ = builder.WriteTexture(out_denoised_img_name, params, Ren::eResState::UnorderedAccess,
                                                 Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRBlur::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_blur_prog = sh.LoadProgram(ctx, "ssr_blur", "internal/ssr_blur.comp.glsl");
        assert(ssr_blur_prog->ready());

        if (!pi_ssr_blur_.Init(ctx.api_ctx(), std::move(ssr_blur_prog), ctx.log())) {
            ctx.log()->Error("RpSSRBlur: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRBlur::Execute(RpBuilder &builder) {
    RpAllocTex &rough_tex = builder.GetReadTexture(rough_tex_);
    RpAllocTex &refl_tex = builder.GetReadTexture(refl_tex_);
    RpAllocBuf &tile_metadata_mask_buf = builder.GetReadBuffer(tile_metadata_mask_buf_);

    RpAllocTex &out_denoised_tex = builder.GetWriteTexture(out_denoised_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRBlur::ROUGH_TEX_SLOT, *rough_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRBlur::REFL_TEX_SLOT, *refl_tex.ref},
        {Ren::eBindTarget::SBuf, SSRBlur::TILE_METADATA_MASK_SLOT, *tile_metadata_mask_buf.ref},
        {Ren::eBindTarget::Image, SSRBlur::OUT_DENOISED_IMG_SLOT, *out_denoised_tex.ref}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + SSRBlur::LOCAL_GROUP_SIZE_X - 1u) / SSRBlur::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + SSRBlur::LOCAL_GROUP_SIZE_Y - 1u) / SSRBlur::LOCAL_GROUP_SIZE_Y, 1u};

    SSRBlur::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{glossy_thres_, mirror_thres_};

    Ren::DispatchCompute(pi_ssr_blur_, grp_count, bindings, COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                         builder.ctx().default_descr_alloc(), builder.ctx().log());
}
#endif