#include "RpSSRResolveSpatial.h"

#include <Ren/Context.h>
#include <Ren/ProbeStorage.h>
#include <Ren/Program.h>
#include <Ren/RastState.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_resolve_spatial_interface.glsl"

void RpSSRResolveSpatial::Setup(RpBuilder &builder, const ViewState *view_state, const char depth_tex_name[],
                                const char norm_tex_name[], const char rough_tex_name[], const char refl_tex_name[],
                                const char tile_metadata_mask_name[], const char denoised_img_name[]) {
    view_state_ = view_state;

    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    rough_tex_ =
        builder.ReadTexture(rough_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    relf_tex_ =
        builder.ReadTexture(refl_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    tile_metadata_mask_buf_ = builder.ReadBuffer(tile_metadata_mask_name, Ren::eResState::ShaderResource,
                                                 Ren::eStageBits::ComputeShader, *this);

    { // Reflection color texture
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

void RpSSRResolveSpatial::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef ssr_resolve_spacial_prog =
            sh.LoadProgram(ctx, "ssr_resolve_spatial", "internal/ssr_resolve_spatial.comp.glsl");
        assert(ssr_resolve_spacial_prog->ready());

        if (!pi_ssr_resolve_spatial_.Init(ctx.api_ctx(), std::move(ssr_resolve_spacial_prog), ctx.log())) {
            ctx.log()->Error("RpSSRResolveSpatial: failed to initialize pipeline!");
        }

        initialized = true;
    }
}

void RpSSRResolveSpatial::Execute(RpBuilder &builder) {
    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &rough_tex = builder.GetReadTexture(rough_tex_);
    RpAllocTex &relf_tex = builder.GetReadTexture(relf_tex_);
    RpAllocBuf &tile_metadata_mask_buf = builder.GetReadBuffer(tile_metadata_mask_buf_);

    RpAllocTex &out_denoised_tex = builder.GetWriteTexture(out_denoised_tex_);

    LazyInit(builder.ctx(), builder.sh());

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRResolveSpacial::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveSpacial::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveSpacial::ROUGH_TEX_SLOT, *rough_tex.ref},
        {Ren::eBindTarget::Tex2D, SSRResolveSpacial::REFL_TEX_SLOT, *relf_tex.ref},
        {Ren::eBindTarget::SBuf, SSRResolveSpacial::TILE_METADATA_MASK_SLOT, *tile_metadata_mask_buf.ref},
        {Ren::eBindTarget::Image, SSRResolveSpacial::OUT_DENOISED_IMG_SLOT, *out_denoised_tex.ref}};

    const auto grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRResolveSpacial::LOCAL_GROUP_SIZE_X - 1u) / SSRResolveSpacial::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRResolveSpacial::LOCAL_GROUP_SIZE_Y - 1u) / SSRResolveSpacial::LOCAL_GROUP_SIZE_Y,
        1u};

    SSRResolveSpacial::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{GLOSSY_THRESHOLD, MIRROR_THRESHOLD};

    Ren::DispatchCompute(pi_ssr_resolve_spatial_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}
