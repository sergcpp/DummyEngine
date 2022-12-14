#include "Renderer.h"

#include <Ren/Context.h>

#include "../assets/shaders/internal/rt_shadow_classify_interface.h"
#include "../assets/shaders/internal/rt_shadow_classify_tiles_interface.h"
#include "../assets/shaders/internal/rt_shadow_debug_interface.h"
#include "../assets/shaders/internal/rt_shadow_filter_interface.h"
#include "../assets/shaders/internal/rt_shadow_prepare_mask_interface.h"
#include "../assets/shaders/internal/rt_shadows_interface.h"
#include "../assets/shaders/internal/sun_shadows_interface.h"

void Renderer::AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, RpResRef rt_obj_instances_res,
                                     FrameTextures &frame_textures, const bool debug_denoise) {
    RpResRef indir_args;

    { // Prepare atomic counter
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

            const auto grp_count = Ren::Vec3u{(view_state_.act_res[0] + RTShadowClassify::LOCAL_GROUP_SIZE_X - 1u) /
                                                  RTShadowClassify::LOCAL_GROUP_SIZE_X,
                                              (view_state_.act_res[1] + RTShadowClassify::LOCAL_GROUP_SIZE_Y - 1u) /
                                                  RTShadowClassify::LOCAL_GROUP_SIZE_Y,
                                              1u};

            RTShadowClassify::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.frame_index = view_state_.frame_index;

            Ren::DispatchCompute(pi_shadow_classify_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
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
        data->ndx_buf = rt_shadows.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
        data->shared_data = rt_shadows.AddUniformBufferInput(common_buffers.shared_data_res, stage);
        data->noise_tex = rt_shadows.AddTextureInput(noise_tex, stage);
        data->depth_tex = rt_shadows.AddTextureInput(frame_textures.depth, stage);
        data->normal_tex = rt_shadows.AddTextureInput(frame_textures.normal, stage);
        data->tlas_buf = rt_shadows.AddStorageReadonlyInput(acc_struct_data.rt_sh_tlas_buf, stage);
        data->tile_list_buf = rt_shadows.AddStorageReadonlyInput(tile_list, stage);
        data->indir_args = rt_shadows.AddIndirectBufferInput(indir_args);

        data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Shadow)];

        ray_hits_tex = data->out_shadow_tex = rt_shadows.AddStorageImageOutput(ray_hits_tex, stage);

        if (!ctx_.capabilities.raytracing) {
            data->swrt.root_node = persistent_data.swrt.rt_root_node;
            data->swrt.blas_buf = rt_shadows.AddStorageReadonlyInput(persistent_data.rt_blas_buf, stage);
            data->swrt.prim_ndx_buf =
                rt_shadows.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
            data->swrt.meshes_buf = rt_shadows.AddStorageReadonlyInput(persistent_data.swrt.rt_meshes_buf, stage);
            data->swrt.mesh_instances_buf = rt_shadows.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(USE_GL_RENDER)
            data->swrt.textures_buf = rt_shadows.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
        }

        rp_rt_shadows_.Setup(rp_builder_, &view_state_, &bindless, data);
        rt_shadows.set_executor(&rp_rt_shadows_);
    }

    if (debug_denoise) {
        auto &rt_shadow_debug = rp_builder_.AddPass("RT SH DEBUG");

        struct PassData {
            RpResRef hit_mask_tex;
            RpResRef out_result_img;
        };

        auto *data = rt_shadow_debug.AllocPassData<PassData>();
        data->hit_mask_tex = rt_shadow_debug.AddTextureInput(ray_hits_tex, Ren::eStageBits::ComputeShader);

        { // shadow mask buffer
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR8;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            frame_textures.sun_shadow = data->out_result_img =
                rt_shadow_debug.AddStorageImageOutput("RT SH Debug", params, Ren::eStageBits::ComputeShader);
        }

        rt_shadow_debug.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &hit_mask_tex = builder.GetReadTexture(data->hit_mask_tex);
            RpAllocTex &out_result_img = builder.GetWriteTexture(data->out_result_img);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, RTShadowDebug::HIT_MASK_TEX_SLOT, *hit_mask_tex.ref},
                {Ren::eBindTarget::Image, RTShadowDebug::OUT_RESULT_IMG_SLOT, *out_result_img.ref}};

            const uint32_t x_tiles = (view_state_.act_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.act_res[1] + 4u - 1u) / 4;

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowDebug::LOCAL_GROUP_SIZE_X - 1u) / RTShadowDebug::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowDebug::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowDebug::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_debug_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });

        return;
    }

    RpResRef shadow_mask;

    { // Prepare shadow mask
        auto &rt_prep_mask = rp_builder_.AddPass("RT SH PREPARE");

        struct PassData {
            RpResRef hit_mask_tex;
            RpResRef out_shadow_mask_buf;
        };

        auto *data = rt_prep_mask.AllocPassData<PassData>();
        data->hit_mask_tex = rt_prep_mask.AddTextureInput(ray_hits_tex, Ren::eStageBits::ComputeShader);

        { // shadow mask buffer
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.scr_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.scr_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            shadow_mask = data->out_shadow_mask_buf =
                rt_prep_mask.AddStorageOutput("RT SH Mask", desc, Ren::eStageBits::ComputeShader);
        }

        rt_prep_mask.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &hit_mask_tex = builder.GetReadTexture(data->hit_mask_tex);

            RpAllocBuf &out_shadow_mask_buf = builder.GetWriteBuffer(data->out_shadow_mask_buf);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, RTShadowPrepareMask::HIT_MASK_TEX_SLOT, *hit_mask_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowPrepareMask::SHADOW_MASK_BUF_SLOT, *out_shadow_mask_buf.ref}};

            const uint32_t x_tiles = (view_state_.act_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.act_res[1] + 4u - 1u) / 4;

            const Ren::Vec3u grp_count = Ren::Vec3u{(x_tiles + 4u - 1u) / 4u, (y_tiles + 4u - 1u) / 4u, 1u};

            RTShadowPrepareMask::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_prepare_mask_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef repro_results, tiles_metadata;

    { // Classify tiles
        auto &rt_classify_tiles = rp_builder_.AddPass("RT SH CLASSIFY TILES");

        struct PassData {
            RpResRef depth;
            RpResRef velocity;
            RpResRef normal;
            RpResRef hist;
            RpResRef prev_depth;
            RpResRef prev_moments;
            RpResRef ray_hits;
            RpResRef shared_data;

            RpResRef out_tile_metadata_buf;
            RpResRef out_repro_results_img;
            RpResRef out_moments_img;
        };

        auto *data = rt_classify_tiles.AllocPassData<PassData>();
        data->depth = rt_classify_tiles.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->velocity = rt_classify_tiles.AddTextureInput(frame_textures.velocity, Ren::eStageBits::ComputeShader);
        data->normal = rt_classify_tiles.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->hist = rt_classify_tiles.AddHistoryTextureInput("SH Filter 0 Tex", Ren::eStageBits::ComputeShader);
        data->prev_depth =
            rt_classify_tiles.AddHistoryTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->prev_moments = rt_classify_tiles.AddHistoryTextureInput("SH Moments Tex", Ren::eStageBits::ComputeShader);
        data->ray_hits = rt_classify_tiles.AddStorageReadonlyInput(shadow_mask, Ren::eStageBits::ComputeShader);
        data->shared_data =
            rt_classify_tiles.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);

        { // metadata buffer
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.scr_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.scr_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            tiles_metadata = data->out_tile_metadata_buf =
                rt_classify_tiles.AddStorageOutput("RT SH Tile Meta", desc, Ren::eStageBits::ComputeShader);
        }

        { // reprojected texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            repro_results = data->out_repro_results_img =
                rt_classify_tiles.AddStorageImageOutput("SH Reproj Tex", params, Ren::eStageBits::ComputeShader);
        }

        { // moments texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            data->out_moments_img =
                rt_classify_tiles.AddStorageImageOutput("SH Moments Tex", params, Ren::eStageBits::ComputeShader);
        }

        rt_classify_tiles.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &velocity_tex = builder.GetReadTexture(data->velocity);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &hist_tex = builder.GetReadTexture(data->hist);
            RpAllocTex &prev_depth_tex = builder.GetReadTexture(data->prev_depth);
            RpAllocTex &prev_moments_tex = builder.GetReadTexture(data->prev_moments);
            RpAllocBuf &ray_hits_buf = builder.GetReadBuffer(data->ray_hits);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            RpAllocBuf &out_tile_metadata_buf = builder.GetWriteBuffer(data->out_tile_metadata_buf);
            RpAllocTex &out_repro_results_img = builder.GetWriteTexture(data->out_repro_results_img);
            RpAllocTex &out_moments_img = builder.GetWriteTexture(data->out_moments_img);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::HISTORY_TEX_SLOT, *hist_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::PREV_DEPTH_TEX_SLOT, *prev_depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowClassifyTiles::PREV_MOMENTS_TEX_SLOT, *prev_moments_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowClassifyTiles::RAY_HITS_BUF_SLOT, *ray_hits_buf.ref},
                {Ren::eBindTarget::SBuf, RTShadowClassifyTiles::OUT_TILE_METADATA_BUF_SLOT, *out_tile_metadata_buf.ref},
                {Ren::eBindTarget::Image, RTShadowClassifyTiles::OUT_REPROJ_RESULTS_IMG_SLOT,
                 *out_repro_results_img.ref},
                {Ren::eBindTarget::Image, RTShadowClassifyTiles::OUT_MOMENTS_IMG_SLOT, *out_moments_img.ref},
            };

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + RTShadowClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) /
                               RTShadowClassifyTiles::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + RTShadowClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) /
                               RTShadowClassifyTiles::LOCAL_GROUP_SIZE_Y,
                           1u};

            RTShadowClassifyTiles::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_classify_tiles_, grp_count, bindings, &uniform_params,
                                 sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef output_tex;

    { // Filter shadow 0
        auto &rt_filter = rp_builder_.AddPass("RT SH FILTER 0");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef input;
            RpResRef tile_metadata;
            RpResRef shared_data;

            RpResRef out_history_img;
        };

        auto *data = rt_filter.AllocPassData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->input = rt_filter.AddTextureInput(repro_results, Ren::eStageBits::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Ren::eStageBits::ComputeShader);
        data->shared_data =
            rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            output_tex = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 0 Tex", params, Ren::eStageBits::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &input_tex = builder.GetReadTexture(data->input);
            RpAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            RpAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Ren::eBindTarget::Image, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_filter_[0], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    { // Filter shadow 1
        auto &rt_filter = rp_builder_.AddPass("RT SH FILTER 1");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef input;
            RpResRef tile_metadata;
            RpResRef shared_data;

            RpResRef out_history_img;
        };

        auto *data = rt_filter.AllocPassData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->input = rt_filter.AddTextureInput(output_tex, Ren::eStageBits::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Ren::eStageBits::ComputeShader);
        data->shared_data =
            rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            output_tex = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 1 Tex", params, Ren::eStageBits::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &input_tex = builder.GetReadTexture(data->input);
            RpAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            RpAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Ren::eBindTarget::Image, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_filter_[1], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    { // Filter shadow 2
        auto &rt_filter = rp_builder_.AddPass("RT SH FILTER 2");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef input;
            RpResRef tile_metadata;
            RpResRef shared_data;

            RpResRef out_history_img;
        };

        auto *data = rt_filter.AllocPassData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->input = rt_filter.AddTextureInput(output_tex, Ren::eStageBits::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Ren::eStageBits::ComputeShader);
        data->shared_data =
            rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR8;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            output_tex = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 2 Tex", params, Ren::eStageBits::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &input_tex = builder.GetReadTexture(data->input);
            RpAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            RpAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Ren::eBindTarget::SBuf, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Ren::eBindTarget::Image, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            Ren::DispatchCompute(pi_shadow_filter_[2], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    frame_textures.sun_shadow = output_tex;
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

        Ren::DispatchCompute(pi_sun_shadows_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                             builder.ctx().default_descr_alloc(), builder.ctx().log());
    });

    frame_textures.sun_shadow = shadow_tex;
}
