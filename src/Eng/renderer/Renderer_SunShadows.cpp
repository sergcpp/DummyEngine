#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/rt_shadow_classify_interface.h"
#include "shaders/rt_shadow_classify_tiles_interface.h"
#include "shaders/rt_shadow_debug_interface.h"
#include "shaders/rt_shadow_filter_interface.h"
#include "shaders/rt_shadow_prepare_mask_interface.h"
#include "shaders/sun_shadows_interface.h"

void Eng::Renderer::AddHQSunShadowsPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                          const AccelerationStructureData &acc_struct_data,
                                          const BindlessTextureData &bindless, FgResRef rt_geo_instances_res,
                                          FgResRef rt_obj_instances_res, FrameTextures &frame_textures,
                                          const bool debug_denoise) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    const bool EnableFilter = (settings.taa_mode != eTAAMode::Static);

    FgResRef indir_args;

    { // Prepare atomic counter
        auto &rt_sh_prepare = fg_builder_.AddNode("RT SH PREPARE");

        struct PassData {
            FgResRef tile_counter;
        };

        auto *data = rt_sh_prepare.AllocNodeData<PassData>();

        { // tile counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = sizeof(Ren::DispatchIndirectCommand);

            indir_args = data->tile_counter = rt_sh_prepare.AddTransferOutput("SH Tile Counter", desc);
        }

        rt_sh_prepare.set_execute_cb([data](FgBuilder &builder) {
            FgAllocBuf &tile_counter_buf = builder.GetWriteBuffer(data->tile_counter);

            Ren::DispatchIndirectCommand indirect_cmd = {};
            indirect_cmd.num_groups_x = 0; // will be incremented atomically
            indirect_cmd.num_groups_y = 1;
            indirect_cmd.num_groups_z = 1;

            Ren::Context &ctx = builder.ctx();
            tile_counter_buf.ref->UpdateImmediate(0, sizeof(indirect_cmd), &indirect_cmd, ctx.current_cmd_buf());
        });
    }

    FgResRef tile_list, ray_hits_tex, noise_tex;

    { // Classify tiles
        auto &rt_shadows = fg_builder_.AddNode("RT SH CLASSIFY");

        struct PassData {
            FgResRef depth;
            FgResRef normal;
            FgResRef shared_data;
            FgResRef tile_counter;
            FgResRef tile_list;
            FgResRef sobol, scrambling_tile, ranking_tile;
            FgResRef out_ray_hits_tex, out_noise_tex;
        };

        auto *data = rt_shadows.AllocNodeData<PassData>();
        data->depth = rt_shadows.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_shadows.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->shared_data = rt_shadows.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        indir_args = data->tile_counter = rt_shadows.AddStorageOutput(indir_args, Stg::ComputeShader);
        data->sobol = rt_shadows.AddStorageReadonlyInput(sobol_seq_buf_, Stg::ComputeShader);
        data->scrambling_tile = rt_shadows.AddStorageReadonlyInput(scrambling_tile_buf_, Stg::ComputeShader);
        data->ranking_tile = rt_shadows.AddStorageReadonlyInput(ranking_tile_buf_, Stg::ComputeShader);

        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 3) / 4) * 4 * sizeof(uint32_t);

            tile_list = data->tile_list = rt_shadows.AddStorageOutput("SH Tile List", desc, Stg::ComputeShader);
        }
        { // ray hits texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 3) / 4;
            params.format = Ren::eTexFormat::R32UI;
            params.sampling.filter = Ren::eTexFilter::Nearest;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            ray_hits_tex = data->out_ray_hits_tex =
                rt_shadows.AddStorageImageOutput("SH Ray Hits", params, Stg::ComputeShader);
        }
        { // blue noise texture
            Ren::Tex2DParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RG8;
            params.sampling.filter = Ren::eTexFilter::Nearest;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex = rt_shadows.AddStorageImageOutput("SH BN Tex", params, Stg::ComputeShader);
        }

        rt_shadows.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            FgAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            FgAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            FgAllocBuf &tile_counter_buf = builder.GetWriteBuffer(data->tile_counter);
            FgAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            FgAllocTex &ray_hits_tex = builder.GetWriteTexture(data->out_ray_hits_tex);
            FgAllocTex &noise_tex = builder.GetWriteTexture(data->out_noise_tex);

            // Initialize texel buffers if needed
            if (!sobol_buf.tbos[0]) {
                sobol_buf.tbos[0] = ctx_.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::R32UI, 0,
                                                         sobol_buf.ref->size());
            }
            if (!scrambling_tile_buf.tbos[0]) {
                scrambling_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::R32UI, 0,
                                         scrambling_tile_buf.ref->size());
            }
            if (!ranking_tile_buf.tbos[0]) {
                ranking_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref, Ren::eTexFormat::R32UI, 0,
                                         ranking_tile_buf.ref->size());
            }

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, RTShadowClassify::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowClassify::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::SBufRO, RTShadowClassify::TILE_COUNTER_SLOT, *tile_counter_buf.ref},
                {Trg::SBufRO, RTShadowClassify::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Trg::UTBuf, RTShadowClassify::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Trg::UTBuf, RTShadowClassify::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Trg::UTBuf, RTShadowClassify::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Trg::Image2D, RTShadowClassify::RAY_HITS_IMG_SLOT, *ray_hits_tex.ref},
                {Trg::Image2D, RTShadowClassify::NOISE_IMG_SLOT, *noise_tex.ref}};

            const auto grp_count = Ren::Vec3u{(view_state_.act_res[0] + RTShadowClassify::LOCAL_GROUP_SIZE_X - 1u) /
                                                  RTShadowClassify::LOCAL_GROUP_SIZE_X,
                                              (view_state_.act_res[1] + RTShadowClassify::LOCAL_GROUP_SIZE_Y - 1u) /
                                                  RTShadowClassify::LOCAL_GROUP_SIZE_Y,
                                              1u};

            RTShadowClassify::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.frame_index = view_state_.frame_index;

            DispatchCompute(*pi_shadow_classify_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    { // Trace shadow rays
        auto &rt_shadows = fg_builder_.AddNode("RT SUN SHADOWS");

        auto *data = rt_shadows.AllocNodeData<ExRTShadows::Args>();

        const Ren::eStageBits stage = Stg::ComputeShader;

        data->geo_data = rt_shadows.AddStorageReadonlyInput(rt_geo_instances_res, stage);
        data->materials = rt_shadows.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
        data->vtx_buf1 = rt_shadows.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
        data->ndx_buf = rt_shadows.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
        data->shared_data = rt_shadows.AddUniformBufferInput(common_buffers.shared_data_res, stage);
        data->noise_tex = rt_shadows.AddTextureInput(noise_tex, stage);
        data->depth_tex = rt_shadows.AddTextureInput(frame_textures.depth, stage);
        data->normal_tex = rt_shadows.AddTextureInput(frame_textures.normal, stage);
        data->tlas_buf = rt_shadows.AddStorageReadonlyInput(acc_struct_data.rt_sh_tlas_buf, stage);
        data->tile_list_buf = rt_shadows.AddStorageReadonlyInput(tile_list, stage);
        data->indir_args = rt_shadows.AddIndirectBufferInput(indir_args);

        data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Shadow)];

        ray_hits_tex = data->out_shadow_tex = rt_shadows.AddStorageImageOutput(ray_hits_tex, stage);

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = persistent_data.swrt.rt_root_node;
            data->swrt.blas_buf = rt_shadows.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
            data->swrt.prim_ndx_buf =
                rt_shadows.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
            data->swrt.mesh_instances_buf = rt_shadows.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(REN_GL_BACKEND)
            data->swrt.textures_buf = rt_shadows.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
        }

        ex_rt_shadows_.Setup(fg_builder_, &view_state_, &bindless, data);
        rt_shadows.set_executor(&ex_rt_shadows_);
    }

    if (debug_denoise) {
        auto &rt_shadow_debug = fg_builder_.AddNode("RT SH DEBUG");

        struct PassData {
            FgResRef hit_mask_tex;
            FgResRef out_result_img;
        };

        auto *data = rt_shadow_debug.AllocNodeData<PassData>();
        data->hit_mask_tex = rt_shadow_debug.AddTextureInput(ray_hits_tex, Stg::ComputeShader);

        { // shadow mask buffer
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            frame_textures.sun_shadow = data->out_result_img =
                rt_shadow_debug.AddStorageImageOutput("RT SH Debug", params, Stg::ComputeShader);
        }

        rt_shadow_debug.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &hit_mask_tex = builder.GetReadTexture(data->hit_mask_tex);
            FgAllocTex &out_result_img = builder.GetWriteTexture(data->out_result_img);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, RTShadowDebug::HIT_MASK_TEX_SLOT, *hit_mask_tex.ref},
                                             {Trg::Image2D, RTShadowDebug::OUT_RESULT_IMG_SLOT, *out_result_img.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowDebug::LOCAL_GROUP_SIZE_X - 1u) / RTShadowDebug::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowDebug::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowDebug::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            DispatchCompute(*pi_shadow_debug_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });

        return;
    }

    FgResRef shadow_mask;

    { // Prepare shadow mask
        auto &rt_prep_mask = fg_builder_.AddNode("RT SH PREPARE");

        struct PassData {
            FgResRef hit_mask_tex;
            FgResRef out_shadow_mask_buf;
        };

        auto *data = rt_prep_mask.AllocNodeData<PassData>();
        data->hit_mask_tex = rt_prep_mask.AddTextureInput(ray_hits_tex, Stg::ComputeShader);

        { // shadow mask buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.scr_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.scr_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            shadow_mask = data->out_shadow_mask_buf =
                rt_prep_mask.AddStorageOutput("RT SH Mask", desc, Stg::ComputeShader);
        }

        rt_prep_mask.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &hit_mask_tex = builder.GetReadTexture(data->hit_mask_tex);

            FgAllocBuf &out_shadow_mask_buf = builder.GetWriteBuffer(data->out_shadow_mask_buf);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DSampled, RTShadowPrepareMask::HIT_MASK_TEX_SLOT, *hit_mask_tex.ref},
                {Trg::SBufRW, RTShadowPrepareMask::SHADOW_MASK_BUF_SLOT, *out_shadow_mask_buf.ref}};

            const uint32_t x_tiles = (view_state_.act_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.act_res[1] + 4u - 1u) / 4;

            const Ren::Vec3u grp_count = Ren::Vec3u{(x_tiles + 4u - 1u) / 4u, (y_tiles + 4u - 1u) / 4u, 1u};

            RTShadowPrepareMask::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            DispatchCompute(*pi_shadow_prepare_mask_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef repro_results, tiles_metadata;

    { // Classify tiles
        auto &rt_classify_tiles = fg_builder_.AddNode("RT SH CLASSIFY TILES");

        struct PassData {
            FgResRef depth;
            FgResRef velocity;
            FgResRef normal;
            FgResRef hist;
            FgResRef prev_depth;
            FgResRef prev_moments;
            FgResRef ray_hits;
            FgResRef shared_data;

            FgResRef out_tile_metadata_buf;
            FgResRef out_repro_results_img;
            FgResRef out_moments_img;
        };

        auto *data = rt_classify_tiles.AllocNodeData<PassData>();
        data->depth = rt_classify_tiles.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->velocity = rt_classify_tiles.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->normal = rt_classify_tiles.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        if (EnableFilter) {
            data->hist = rt_classify_tiles.AddHistoryTextureInput("SH Filter 0 Tex", Stg::ComputeShader);
        } else {
            data->hist = rt_classify_tiles.AddHistoryTextureInput("SH Reproj Tex", Stg::ComputeShader);
        }
        data->prev_depth = rt_classify_tiles.AddHistoryTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->prev_moments = rt_classify_tiles.AddHistoryTextureInput("SH Moments Tex", Stg::ComputeShader);
        data->ray_hits = rt_classify_tiles.AddStorageReadonlyInput(shadow_mask, Stg::ComputeShader);
        data->shared_data = rt_classify_tiles.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);

        { // metadata buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.scr_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.scr_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            tiles_metadata = data->out_tile_metadata_buf =
                rt_classify_tiles.AddStorageOutput("RT SH Tile Meta", desc, Stg::ComputeShader);
        }

        { // reprojected texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RG16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            repro_results = data->out_repro_results_img =
                rt_classify_tiles.AddStorageImageOutput("SH Reproj Tex", params, Stg::ComputeShader);
        }

        { // moments texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RG11F_B10F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            data->out_moments_img =
                rt_classify_tiles.AddStorageImageOutput("SH Moments Tex", params, Stg::ComputeShader);
        }

        rt_classify_tiles.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocTex &hist_tex = builder.GetReadTexture(data->hist);
            FgAllocTex &prev_depth_tex = builder.GetReadTexture(data->prev_depth);
            FgAllocTex &prev_moments_tex = builder.GetReadTexture(data->prev_moments);
            FgAllocBuf &ray_hits_buf = builder.GetReadBuffer(data->ray_hits);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            FgAllocBuf &out_tile_metadata_buf = builder.GetWriteBuffer(data->out_tile_metadata_buf);
            FgAllocTex &out_repro_results_img = builder.GetWriteTexture(data->out_repro_results_img);
            FgAllocTex &out_moments_img = builder.GetWriteTexture(data->out_moments_img);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::HISTORY_TEX_SLOT, *hist_tex.ref},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::PREV_DEPTH_TEX_SLOT, {*prev_depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowClassifyTiles::PREV_MOMENTS_TEX_SLOT, *prev_moments_tex.ref},
                {Trg::SBufRO, RTShadowClassifyTiles::RAY_HITS_BUF_SLOT, *ray_hits_buf.ref},
                {Trg::SBufRW, RTShadowClassifyTiles::OUT_TILE_METADATA_BUF_SLOT, *out_tile_metadata_buf.ref},
                {Trg::Image2D, RTShadowClassifyTiles::OUT_REPROJ_RESULTS_IMG_SLOT, *out_repro_results_img.ref},
                {Trg::Image2D, RTShadowClassifyTiles::OUT_MOMENTS_IMG_SLOT, *out_moments_img.ref},
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

            DispatchCompute(*pi_shadow_classify_tiles_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef filtered_result0;

    { // Filter shadow 0
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 0");

        struct PassData {
            FgResRef depth;
            FgResRef normal;
            FgResRef input;
            FgResRef tile_metadata;
            FgResRef shared_data;

            FgResRef out_history_img;
        };

        auto *data = rt_filter.AllocNodeData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(repro_results, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RG16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            filtered_result0 = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 0 Tex", params, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input);
            FgAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            FgAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, RTShadowFilter::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Trg::Image2D, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            DispatchCompute(*pi_shadow_filter_[0], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef filtered_result1;

    { // Filter shadow 1
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 1");

        struct PassData {
            FgResRef depth;
            FgResRef normal;
            FgResRef input;
            FgResRef tile_metadata;
            FgResRef shared_data;

            FgResRef out_history_img;
        };

        auto *data = rt_filter.AllocNodeData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(filtered_result0, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RG16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            filtered_result1 = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 1 Tex", params, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input);
            FgAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            FgAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, RTShadowFilter::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Trg::Image2D, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            DispatchCompute(*pi_shadow_filter_[1], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef filtered_result2;

    { // Filter shadow 2
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 2");

        struct PassData {
            FgResRef depth;
            FgResRef normal;
            FgResRef input;
            FgResRef tile_metadata;
            FgResRef shared_data;

            FgResRef out_history_img;
        };

        auto *data = rt_filter.AllocNodeData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(filtered_result1, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);

        { // out result texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            filtered_result2 = data->out_history_img =
                rt_filter.AddStorageImageOutput("SH Filter 2 Tex", params, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input);
            FgAllocBuf &tile_metadata_buf = builder.GetReadBuffer(data->tile_metadata);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);

            FgAllocTex &out_history_img = builder.GetWriteTexture(data->out_history_img);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, RTShadowFilter::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, RTShadowFilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, RTShadowFilter::INPUT_TEX_SLOT, *input_tex.ref},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, *tile_metadata_buf.ref},
                {Trg::Image2D, RTShadowFilter::OUT_RESULT_IMG_SLOT, *out_history_img.ref},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + RTShadowFilter::LOCAL_GROUP_SIZE_X - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + RTShadowFilter::LOCAL_GROUP_SIZE_Y - 1u) / RTShadowFilter::LOCAL_GROUP_SIZE_Y,
                1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            DispatchCompute(*pi_shadow_filter_[2], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    if (EnableFilter) {
        frame_textures.sun_shadow = filtered_result2;
    } else {
        frame_textures.sun_shadow = repro_results;
    }
}

void Eng::Renderer::AddLQSunShadowsPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                        const AccelerationStructureData &acc_struct_data,
                                        const BindlessTextureData &bindless, const bool enabled,
                                        FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    auto &sun_shadows = fg_builder_.AddNode("SUN SHADOWS");

    struct PassData {
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef shadow_tex;

        FgResRef out_shadow_tex;
    };

    FgResRef shadow_tex;

    auto *data = sun_shadows.AllocNodeData<PassData>();
    data->shared_data = sun_shadows.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
    data->depth_tex = sun_shadows.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
    data->normal_tex = sun_shadows.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
    data->shadow_tex = sun_shadows.AddTextureInput(frame_textures.shadowmap, Stg::ComputeShader);

    { // shadows texture
        Ren::Tex2DParams params;
        params.w = view_state_.scr_res[0];
        params.h = view_state_.scr_res[1];
        params.format = Ren::eTexFormat::R8;
        params.sampling.filter = Ren::eTexFilter::Bilinear;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        shadow_tex = data->out_shadow_tex =
            sun_shadows.AddStorageImageOutput("Sun Shadows", params, Stg::ComputeShader);
    }

    sun_shadows.set_execute_cb([this, data, enabled](FgBuilder &builder) {
        FgAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
        FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        FgAllocTex &norm_tex = builder.GetReadTexture(data->normal_tex);
        FgAllocTex &shadow_tex = builder.GetReadTexture(data->shadow_tex);
        FgAllocTex &out_shadow_tex = builder.GetWriteTexture(data->out_shadow_tex);

        const Ren::Binding bindings[] = {
            {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *shared_data_buf.ref},
            {Trg::Tex2DSampled, SunShadows::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Trg::Tex2DSampled, SunShadows::NORM_TEX_SLOT, *norm_tex.ref},
            {Trg::Tex2DSampled, SunShadows::SHADOW_TEX_SLOT, *shadow_tex.ref},
            {Trg::Tex2DSampled, SunShadows::SHADOW_TEX_VAL_SLOT, {*shadow_tex.ref, *nearest_sampler_}},
            {Trg::Image2D, SunShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

        const Ren::Vec3u grp_count = Ren::Vec3u{
            (view_state_.act_res[0] + SunShadows::LOCAL_GROUP_SIZE_X - 1u) / SunShadows::LOCAL_GROUP_SIZE_X,
            (view_state_.act_res[1] + SunShadows::LOCAL_GROUP_SIZE_Y - 1u) / SunShadows::LOCAL_GROUP_SIZE_Y, 1u};

        SunShadows::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
        uniform_params.enabled = enabled ? 1.0f : 0.0f;
        uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
        uniform_params.softness_factor =
            std::tan(p_list_->env.sun_angle * Ren::Pi<float>() / 180.0f) / 2.0f * p_list_->sun_shadow_bounds;
        uniform_params.softness_factor /= 2.0f * p_list_->sun_shadow_bounds;
        uniform_params.softness_factor *= 0.5f * float(SUN_SHADOW_RES);

        DispatchCompute(*pi_sun_shadows_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                        builder.ctx().default_descr_alloc(), builder.ctx().log());
    });

    frame_textures.sun_shadow = shadow_tex;
}
