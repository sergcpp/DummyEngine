#include "Renderer.h"

#include "../assets/shaders/internal/rt_shadow_classify_interface.glsl"
#include "../assets/shaders/internal/rt_shadows_interface.glsl"
#include "../assets/shaders/internal/sun_shadows_interface.glsl"

void Renderer::AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    RpResRef shadow_tex;

    RpResRef indir_args;

    { // Prepare atomic counter and ray length texture
        auto &rt_sh_prepare = rp_builder_.AddPass("RT SH PREPARE");

        struct PassData {
            RpResRef tile_counter;
        };

        auto *data = rt_sh_prepare.AllocPassData<PassData>();

        { // tile counter
            RpBufDesc desc;
            desc.type = Ren::eBufType::Indirect;
            desc.size = sizeof(Ren::DispatchIndirectCommand);

            indir_args = data->tile_counter = rt_sh_prepare.AddTransferOutput("SH Tile Counter", desc);
        }

        rt_sh_prepare.set_execute_cb([data](RpBuilder &builder) {
            RpAllocBuf &tile_counter_buf = builder.GetWriteBuffer(data->tile_counter);

            Ren::DispatchIndirectCommand indirect_cmd;
            indirect_cmd.num_groups_x = 0; // will be incremented atomically
            indirect_cmd.num_groups_y = 1;
            indirect_cmd.num_groups_z = 1;

            Ren::Context &ctx = builder.ctx();
            tile_counter_buf.ref->UpdateImmediate(0, sizeof(indirect_cmd), &indirect_cmd, ctx.current_cmd_buf());
        });
    }

    RpResRef tile_list, ray_hits_tex, noise_tex;

    { // Classify tiles
        auto &rt_shadows = rp_builder_.AddPass("RT SH CLASSIFY");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef shared_data;
            RpResRef tile_counter;
            RpResRef tile_list;
            RpResRef sobol, scrambling_tile, ranking_tile;
            RpResRef out_ray_hits_tex, out_noise_tex;
        };

        auto *data = rt_shadows.AllocPassData<PassData>();
        data->depth = rt_shadows.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->normal = rt_shadows.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->shared_data =
            rt_shadows.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        indir_args = data->tile_counter = rt_shadows.AddStorageOutput(indir_args, Ren::eStageBits::ComputeShader);
        data->sobol = rt_shadows.AddStorageReadonlyInput(sobol_seq_buf_, Ren::eStageBits::ComputeShader);
        data->scrambling_tile =
            rt_shadows.AddStorageReadonlyInput(scrambling_tile_1spp_buf_, Ren::eStageBits::ComputeShader);
        data->ranking_tile = rt_shadows.AddStorageReadonlyInput(ranking_tile_1spp_buf_, Ren::eStageBits::ComputeShader);

        { // tile list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 3) / 4) * 4 * sizeof(uint32_t);

            tile_list = data->tile_list =
                rt_shadows.AddStorageOutput("SH Tile List", desc, Ren::eStageBits::ComputeShader);
        }
        { // ray hits texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 3) / 4;
            params.format = Ren::eTexFormat::RawR32UI;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            ray_hits_tex = data->out_ray_hits_tex =
                rt_shadows.AddStorageImageOutput("SH Ray Hits", params, Ren::eStageBits::ComputeShader);
        }
        { // blue noise texture
            Ren::Tex2DParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RawRG88;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex =
                rt_shadows.AddStorageImageOutput("SH Blue Noise Tex", params, Ren::eStageBits::ComputeShader);
        }

        rt_shadows.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            RpAllocBuf &tile_counter_buf = builder.GetWriteBuffer(data->tile_counter);
            RpAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            RpAllocTex &ray_hits_tex = builder.GetWriteTexture(data->out_ray_hits_tex);
            RpAllocTex &noise_tex = builder.GetWriteTexture(data->out_noise_tex);

            // Initialize texel buffers if needed
            if (!sobol_buf.tbos[0]) {
                sobol_buf.tbos[0] = ctx_.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::RawR32UI,
                                                         0, sobol_buf.ref->size());
            }
            if (!scrambling_tile_buf.tbos[0]) {
                scrambling_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::RawR32UI,
                                         0, scrambling_tile_buf.ref->size());
            }
            if (!ranking_tile_buf.tbos[0]) {
                ranking_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                         ranking_tile_buf.ref->size());
            }

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassify::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassify::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowClassify::TILE_COUNTER_SLOT, *tile_counter_buf.ref},
                {Ren::eBindTarget::SBuf, RTShadowClassify::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::TBuf, RTShadowClassify::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, RTShadowClassify::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, RTShadowClassify::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Ren::eBindTarget::Image, RTShadowClassify::RAY_HITS_IMG_SLOT, *ray_hits_tex.ref},
                {Ren::eBindTarget::Image, RTShadowClassify::NOISE_IMG_SLOT, *noise_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + RTShadowClassify::LOCAL_GROUP_SIZE_X - 1u) /
                               RTShadowClassify::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + RTShadowClassify::LOCAL_GROUP_SIZE_Y - 1u) /
                               RTShadowClassify::LOCAL_GROUP_SIZE_Y,
                           1u};

            RTShadowClassify::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.frame_index = view_state_.frame_index;

            Ren::DispatchCompute(pi_shadow_classify_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                                 sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    { // Trace shadow rays
        auto &rt_shadows = rp_builder_.AddPass("RT SUN SHADOWS");

        auto *data = rt_shadows.AllocPassData<RpRTShadowsData>();

        const Ren::eStageBits stage =
            ctx_.capabilities.ray_query ? Ren::eStageBits::ComputeShader : Ren::eStageBits::RayTracingShader;

        data->geo_data = rt_shadows.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stage);
        data->materials = rt_shadows.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
        data->vtx_buf1 = rt_shadows.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
        data->vtx_buf2 = rt_shadows.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
        data->ndx_buf = rt_shadows.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
        data->shared_data = rt_shadows.AddUniformBufferInput(common_buffers.shared_data_res, stage);
        data->noise_tex = rt_shadows.AddTextureInput(noise_tex, stage);
        data->depth_tex = rt_shadows.AddTextureInput(frame_textures.depth, stage);
        data->normal_tex = rt_shadows.AddTextureInput(frame_textures.normal, stage);
        // data->env_tex = rt_shadows.AddTextureInput(env_map, stage);
        // data->ray_counter = rt_shadows.AddStorageReadonlyInput(ray_counter, stage);
        // data->ray_list = rt_shadows.AddStorageReadonlyInput(ray_rt_list, stage);
        // data->indir_args = rt_shadows.AddIndirectBufferInput(indir_rt_disp_buf);
        data->tlas_buf = rt_shadows.AddStorageReadonlyInput(acc_struct_data.rt_sh_tlas_buf, stage);
        data->tile_list_buf = rt_shadows.AddStorageReadonlyInput(tile_list, stage);
        data->indir_args = rt_shadows.AddIndirectBufferInput(indir_args);

        shadow_tex = data->out_shadow_tex = rt_shadows.AddStorageImageOutput(ray_hits_tex, stage);

        rp_rt_shadows_.Setup(rp_builder_, &view_state_, &acc_struct_data, &bindless, data);
        rt_shadows.set_executor(&rp_rt_shadows_);
    }

    frame_textures.sun_shadow = shadow_tex;
}

void Renderer::AddLQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    auto &sun_shadows = rp_builder_.AddPass("SUN SHADOWS");

    struct RpShadowsData {
        RpResRef shared_data;
        RpResRef depth_tex;
        RpResRef normal_tex;
        RpResRef shadow_tex;

        RpResRef out_shadow_tex;
    };

    RpResRef shadow_tex;

    auto *data = sun_shadows.AllocPassData<RpShadowsData>();
    data->shared_data =
        sun_shadows.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
    data->depth_tex = sun_shadows.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
    data->normal_tex = sun_shadows.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
    data->shadow_tex = sun_shadows.AddTextureInput(frame_textures.shadowmap, Ren::eStageBits::ComputeShader);

    { // shadows texture
        Ren::Tex2DParams params;
        params.w = view_state_.scr_res[0];
        params.h = view_state_.scr_res[1];
        params.format = Ren::eTexFormat::RawR8;
        params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        shadow_tex = data->out_shadow_tex =
            sun_shadows.AddStorageImageOutput("Sun Shadows", params, Ren::eStageBits::ComputeShader);
    }

    sun_shadows.set_execute_cb([this, data](RpBuilder &builder) {
        RpAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
        RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        RpAllocTex &norm_tex = builder.GetReadTexture(data->normal_tex);
        RpAllocTex &shadow_tex = builder.GetReadTexture(data->shadow_tex);
        RpAllocTex &out_shadow_tex = builder.GetWriteTexture(data->out_shadow_tex);

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *shared_data_buf.ref},
            {Ren::eBindTarget::Tex2D, SunShadows::DEPTH_TEX_SLOT, *depth_tex.ref},
            {Ren::eBindTarget::Tex2D, SunShadows::NORM_TEX_SLOT, *norm_tex.ref},
            {Ren::eBindTarget::Tex2D, SunShadows::SHADOW_TEX_SLOT, *shadow_tex.ref},
            {Ren::eBindTarget::Image, SunShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

        const Ren::Vec3u grp_count = Ren::Vec3u{
            (view_state_.act_res[0] + SunShadows::LOCAL_GROUP_SIZE_X - 1u) / SunShadows::LOCAL_GROUP_SIZE_X,
            (view_state_.act_res[1] + SunShadows::LOCAL_GROUP_SIZE_Y - 1u) / SunShadows::LOCAL_GROUP_SIZE_Y, 1u};

        SunShadows::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

        Ren::DispatchCompute(pi_sun_shadows_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                             sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
    });

    frame_textures.sun_shadow = shadow_tex;
}
