#include "RpSSRClassifyTiles.h"

#include "../../Utils/ShaderLoader.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/ssr_classify_tiles_interface.glsl"

void RpSSRClassifyTiles::Setup(RpBuilder &builder, const ViewState *view_state, const char spec_tex_name[],
                               const char temp_variance_mask_name[], const char tile_metadata_mask_name[],
                               const char ray_counter_name[], const char ray_list_name[], const char rough_tex_name[]) {
    view_state_ = view_state;

    spec_tex_ =
        builder.ReadTexture(spec_tex_name, Ren::eResState::ShaderResource, Ren::eStageBits::ComputeShader, *this);
    temp_variance_mask_buf_ = builder.ReadBuffer(temp_variance_mask_name, Ren::eResState::UnorderedAccess,
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
    { // tile metadata mask
        RpBufDesc desc;
        desc.type = Ren::eBufType::Storage;
        desc.size = ((view_state->scr_res[0] + 7) / 8) * ((view_state->scr_res[1] + 7) / 8) * sizeof(uint32_t);

        tile_metadata_mask_buf_ = builder.WriteBuffer(tile_metadata_mask_name, desc, Ren::eResState::UnorderedAccess,
                                                      Ren::eStageBits::ComputeShader, *this);
    }
    { // roughness texture
        Ren::Tex2DParams params;
        params.w = view_state->scr_res[0];
        params.h = view_state->scr_res[1];
        params.format = Ren::eTexFormat::RawR8;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        rough_tex_ = builder.WriteTexture(rough_tex_name, params, Ren::eResState::UnorderedAccess,
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

    RpAllocTex &spec_tex = builder.GetReadTexture(spec_tex_);
    RpAllocBuf &temp_variance_mask_buf = builder.GetReadBuffer(temp_variance_mask_buf_);

    RpAllocBuf &tile_metadata_mask_buf = builder.GetWriteBuffer(tile_metadata_mask_buf_);
    RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(ray_counter_buf_);
    RpAllocBuf &ray_list_buf = builder.GetWriteBuffer(ray_list_buf_);
    RpAllocTex &rough_tex = builder.GetWriteTexture(rough_tex_);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::Tex2D, SSRClassifyTiles::SPEC_TEX_SLOT, *spec_tex.ref},
        {Ren::eBindTarget::SBuf, SSRClassifyTiles::TEMP_VARIANCE_MASK_SLOT, *temp_variance_mask_buf.ref},
        {Ren::eBindTarget::SBuf, SSRClassifyTiles::TILE_METADATA_MASK_SLOT, *tile_metadata_mask_buf.ref},
        {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Image, SSRClassifyTiles::ROUGH_IMG_SLOT, *rough_tex.ref}};

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SSRClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) / SSRClassifyTiles::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SSRClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) / SSRClassifyTiles::LOCAL_GROUP_SIZE_Y,
        1u};

    SSRClassifyTiles::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.thresholds = Ren::Vec2f{GLOSSY_THRESHOLD, MIRROR_THRESHOLD};
    uniform_params.samples_and_guided = Ren::Vec2u{4, VARIANCE_GUIDED};

    Ren::DispatchCompute(pi_classify_tiles_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                         sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
}