#include "RpSSRClassifyTiles.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_classify_tiles_interface.glsl"

void RpSSRClassifyTiles::Setup(RpBuilder &builder, const ViewState *view_state, const float glossy_thres,
                               const float mirror_thres, const int sample_count, const bool variance_guided,
                               const char depth_tex_name[], const char norm_tex_name[],
                               Ren::WeakTex2DRef variance_history_tex, const char ray_counter_name[],
                               const char ray_list_name[], const char tile_list_name[], const char refl_tex_name[]) {
    view_state_ = view_state;
    glossy_thres_ = glossy_thres;
    mirror_thres_ = mirror_thres;
    sample_count_ = sample_count;
    variance_guided_ = variance_guided;

    depth_tex_ =
        builder.ReadTexture(depth_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    norm_tex_ =
        builder.ReadTexture(norm_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    variance_history_tex_ = builder.ReadTexture(variance_history_tex, Ren::eResState::ShaderResource,
                                                Ren::eStageBits::ComputeShader, *this);
    ray_counter_buf_ =
        builder.WriteBuffer(ray_counter_name, Ren::eResState::UnorderedAccess, Ren::eStageBits::ComputeShader, *this);

    { // packed ray list
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = view_state->scr_res[0] * view_state->scr_res[1] * sizeof(uint32_t);

        ray_list_buf_ = builder.WriteBuffer(ray_list_name, desc, Ren::eResState::UnorderedAccess,
                                            Ren::eStageBits::ComputeShader, *this);
    }
    { // tile list
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = ((view_state->scr_res[0] + 7) / 8) * ((view_state->scr_res[1] + 7) / 8) * sizeof(uint32_t);

        tile_list_buf_ = builder.WriteBuffer(tile_list_name, desc, Ren::eResState::UnorderedAccess,
                                             Ren::eStageBits::ComputeShader, *this);
    }
    { // reflections texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawRG11F_B10F;
        params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        refl_tex_ = builder.WriteTexture(refl_tex_name, params, Ren::eResState::UnorderedAccess,
                                         Ren::eStageBits::ComputeShader, *this);
    }
}

void RpSSRClassifyTiles::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (initialized) {
        return;
    }

    Ren::ProgramRef classify_tiles_prog =
        sh.LoadProgram(ctx, "ssr_classify_tiles", "internal/ssr_classify_tiles.comp.glsl");
    assert(classify_tiles_prog->ready());

    if (!pi_classify_tiles_.Init(ctx.api_ctx(), std::move(classify_tiles_prog), ctx.log())) {
        ctx.log()->Error("RpSSRClassifyTiles: failed to initialize pipeline!");
    }

    initialized = true;
}

void RpSSRClassifyTiles::Execute(RpBuilder &builder) {
    LazyInit(builder.ctx(), builder.sh());

    RpAllocTex &depth_tex = builder.GetReadTexture(depth_tex_);
    RpAllocTex &norm_tex = builder.GetReadTexture(norm_tex_);
    RpAllocTex &variance_tex = builder.GetReadTexture(variance_history_tex_);

    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocBuf &ray_list_buf = builder.GetWriteBuffer(ray_list_buf_);
    RpAllocBuf &tile_list_buf = builder.GetWriteBuffer(tile_list_buf_);
    RpAllocTex &refl_tex = builder.GetWriteTexture(refl_tex_);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2D, SSRClassifyTiles::DEPTH_TEX_SLOT, *depth_tex.ref},
                                     {Ren::eBindTarget::Tex2D, SSRClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                                     {Ren::eBindTarget::Tex2D, SSRClassifyTiles::VARIANCE_TEX_SLOT, *variance_tex.ref},
                                     {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                     {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
                                     {Ren::eBindTarget::SBuf, SSRClassifyTiles::TILE_LIST_SLOT, *tile_list_buf.ref},
                                     {Ren::eBindTarget::Image, SSRClassifyTiles::REFL_IMG_SLOT, *refl_tex.ref}};

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) / SSRClassifyTiles::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) / SSRClassifyTiles::LOCAL_GROUP_SIZE_Y,
        1u};

    SSRClassifyTiles::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{glossy_thres_, mirror_thres_};
    uniform_params.samples_and_guided = Ren::Vec2u{uint32_t(sample_count_), variance_guided_ ? 1u : 0u};

    Ren::DispatchCompute(pi_classify_tiles_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}
