#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"
#include "executors/ExRTShadows.h"

#include "shaders/rt_shadow_classify_interface.h"
#include "shaders/rt_shadow_classify_tiles_interface.h"
#include "shaders/rt_shadow_debug_interface.h"
#include "shaders/rt_shadow_filter_interface.h"
#include "shaders/rt_shadow_prepare_mask_interface.h"
#include "shaders/sun_shadows_interface.h"

Eng::FgImgRWHandle Eng::Renderer::AddHQSunShadowsPasses(const CommonBuffers &common_buffers,
                                                        const AccelerationStructures &acc_structs,
                                                        const BindlessTextureData &bindless,
                                                        const FgBufROHandle rt_geo_instances_res,
                                                        const FgBufROHandle rt_obj_instances_res,
                                                        const FrameTextures &frame_textures, const bool debug_denoise) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    const bool EnableFilter = (settings.taa_mode != eTAAMode::Static);

    FgBufRWHandle indir_args;

    { // Prepare atomic counter
        auto &rt_sh_prepare = fg_builder_.AddNode("RT SH PREPARE");

        struct PassData {
            FgBufRWHandle tile_counter;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        { // tile counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = sizeof(Ren::DispatchIndirectCommand);

            indir_args = data->tile_counter = rt_sh_prepare.AddTransferOutput("SH Tile Counter", desc);
        }

        rt_sh_prepare.set_execute_cb([data](const FgContext &fg) {
            const Ren::BufferHandle tile_counter = fg.AccessRWBuffer(data->tile_counter);

            Ren::DispatchIndirectCommand indirect_cmd = {};
            indirect_cmd.num_groups_x = 0; // will be incremented atomically
            indirect_cmd.num_groups_y = 1;
            indirect_cmd.num_groups_z = 1;

            const auto &[buf_main, buf_cold] = fg.storages().buffers[tile_counter];
            Ren::Buffer_UpdateInPlace(fg.ren_ctx().api(), buf_main, 0, sizeof(indirect_cmd), &indirect_cmd,
                                      fg.cmd_buf());
        });
    }

    FgBufRWHandle tile_list;
    FgImgRWHandle ray_hits;

    { // Classify tiles
        auto &sh_classify = fg_builder_.AddNode("RT SH CLASSIFY");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle normal;
            FgBufROHandle shared_data;
            FgBufRWHandle tile_counter;
            FgBufRWHandle tile_list;
            FgImgRWHandle out_ray_hits;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = sh_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = sh_classify.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->shared_data = sh_classify.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        indir_args = data->tile_counter = sh_classify.AddStorageOutput(indir_args, Stg::ComputeShader);

        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.ren_res[0] + 7) / 8) * ((view_state_.ren_res[1] + 3) / 4) * 4 * sizeof(uint32_t);

            tile_list = data->tile_list = sh_classify.AddStorageOutput("SH Tile List", desc, Stg::ComputeShader);
        }
        { // ray hits texture
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] + 7) / 8;
            desc.h = (view_state_.ren_res[1] + 3) / 4;
            desc.format = Ren::eFormat::R32UI;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            ray_hits = data->out_ray_hits = sh_classify.AddStorageImageOutput("SH Ray Hits", desc, Stg::ComputeShader);
        }

        sh_classify.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::BufferRWHandle tile_counter = fg.AccessRWBuffer(data->tile_counter);
            const Ren::BufferRWHandle tile_list = fg.AccessRWBuffer(data->tile_list);
            const Ren::ImageRWHandle ray_hits = fg.AccessRWImage(data->out_ray_hits);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::TexSampled, RTShadowClassify::DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, RTShadowClassify::NORM_TEX_SLOT, norm},
                                             {Trg::SBufRW, RTShadowClassify::TILE_COUNTER_SLOT, tile_counter},
                                             {Trg::SBufRW, RTShadowClassify::TILE_LIST_SLOT, tile_list},
                                             {Trg::ImageRW, RTShadowClassify::OUT_RAY_HITS_IMG_SLOT, ray_hits}};

            const auto grp_count = Ren::Vec3u{
                (view_state_.ren_res[0] + RTShadowClassify::GRP_SIZE_X - 1u) / RTShadowClassify::GRP_SIZE_X,
                (view_state_.ren_res[1] + RTShadowClassify::GRP_SIZE_Y - 1u) / RTShadowClassify::GRP_SIZE_Y, 1u};

            RTShadowClassify::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.frame_index = view_state_.frame_index;

            DispatchCompute(pi_shadow_classify_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    { // Trace shadow rays
        auto &rt_shadows = fg_builder_.AddNode("RT SUN SHADOWS");

        const Ren::eStage stage = Stg::ComputeShader;

        auto *data = fg_builder_.AllocTempData<ExRTShadows::Args>();
        data->geo_data = rt_shadows.AddStorageReadonlyInput(rt_geo_instances_res, stage);
        data->materials = rt_shadows.AddStorageReadonlyInput(common_buffers.materials, stage);
        data->vtx_buf1 = rt_shadows.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
        data->ndx_buf = rt_shadows.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
        data->shared_data = rt_shadows.AddUniformBufferInput(common_buffers.shared_data, stage);
        data->tcbn = rt_shadows.AddTextureInput(frame_textures.tcbn_2D_64spp, stage);
        data->depth = rt_shadows.AddTextureInput(frame_textures.depth, stage);
        data->normal = rt_shadows.AddTextureInput(frame_textures.normal, stage);
        data->tlas_buf = rt_shadows.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Shadow)], stage);
        data->tile_list = rt_shadows.AddStorageReadonlyInput(tile_list, stage);
        data->indir_args = rt_shadows.AddIndirectBufferInput(indir_args);

        data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Shadow)];

        ray_hits = data->out_shadow = rt_shadows.AddStorageImageOutput(ray_hits, stage);

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = acc_structs.swrt.rt_root_node;
            data->swrt.blas_buf = rt_shadows.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
            data->swrt.prim_ndx = rt_shadows.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
            data->swrt.mesh_instances = rt_shadows.AddStorageReadonlyInput(rt_obj_instances_res, stage);
        }

        rt_shadows.make_executor<ExRTShadows>(&view_state_, &bindless, data);
    }

    if (debug_denoise) {
        auto &rt_shadow_debug = fg_builder_.AddNode("RT SH DEBUG");

        struct PassData {
            FgImgROHandle hit_mask;
            FgImgRWHandle out_result;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->hit_mask = rt_shadow_debug.AddTextureInput(ray_hits, Stg::ComputeShader);

        { // shadow mask buffer
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            data->out_result = rt_shadow_debug.AddStorageImageOutput("RT SH Debug", desc, Stg::ComputeShader);
        }

        rt_shadow_debug.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle hit_mask = fg.AccessROImage(data->hit_mask);
            const Ren::ImageRWHandle out_result = fg.AccessRWImage(data->out_result);

            const Ren::Binding bindings[] = {{Trg::TexSampled, RTShadowDebug::HIT_MASK_TEX_SLOT, hit_mask},
                                             {Trg::ImageRW, RTShadowDebug::OUT_RESULT_IMG_SLOT, out_result}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + RTShadowDebug::GRP_SIZE_X - 1u) / RTShadowDebug::GRP_SIZE_X,
                           (view_state_.ren_res[1] + RTShadowDebug::GRP_SIZE_Y - 1u) / RTShadowDebug::GRP_SIZE_Y, 1u};

            RTShadowDebug::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.ren_res[0], view_state_.ren_res[1]);

            DispatchCompute(pi_shadow_debug_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });

        return data->out_result;
    }

    FgBufRWHandle shadow_mask;

    { // Prepare shadow mask
        auto &rt_prep_mask = fg_builder_.AddNode("RT SH PREPARE");

        struct PassData {
            FgImgROHandle hit_mask;
            FgBufRWHandle out_shadow_mask;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->hit_mask = rt_prep_mask.AddTextureInput(ray_hits, Stg::ComputeShader);

        { // shadow mask buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.ren_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.ren_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            shadow_mask = data->out_shadow_mask = rt_prep_mask.AddStorageOutput("RT SH Mask", desc, Stg::ComputeShader);
        }

        rt_prep_mask.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle hit_mask = fg.AccessROImage(data->hit_mask);

            const Ren::BufferRWHandle out_shadow_mask = fg.AccessRWBuffer(data->out_shadow_mask);

            const Ren::Binding bindings[] = {{Trg::TexSampled, RTShadowPrepareMask::HIT_MASK_TEX_SLOT, hit_mask},
                                             {Trg::SBufRW, RTShadowPrepareMask::SHADOW_MASK_BUF_SLOT, out_shadow_mask}};

            const uint32_t x_tiles = (view_state_.ren_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.ren_res[1] + 4u - 1u) / 4;

            const Ren::Vec3u grp_count = Ren::Vec3u{(x_tiles + 4u - 1u) / 4u, (y_tiles + 4u - 1u) / 4u, 1u};

            RTShadowPrepareMask::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.ren_res[0], view_state_.ren_res[1]);

            DispatchCompute(pi_shadow_prepare_mask_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle repro_results;
    FgBufRWHandle tiles_metadata;

    { // Classify tiles
        auto &rt_classify_tiles = fg_builder_.AddNode("RT SH CLASSIFY TILES");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle velocity;
            FgImgROHandle normal;
            FgImgROHandle hist;
            FgImgROHandle prev_depth;
            FgImgROHandle prev_moments;
            FgBufROHandle ray_hits;
            FgBufROHandle shared_data;

            FgBufRWHandle out_tile_metadata;
            FgImgRWHandle out_repro_results;
            FgImgRWHandle out_moments;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = rt_classify_tiles.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->velocity = rt_classify_tiles.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->normal = rt_classify_tiles.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        if (EnableFilter) {
            data->hist = rt_classify_tiles.AddHistoryTextureInput("SH Filter 0 Tex", Stg::ComputeShader);
        } else {
            data->hist = rt_classify_tiles.AddHistoryTextureInput("SH Reproj Tex", Stg::ComputeShader);
        }
        data->prev_depth = rt_classify_tiles.AddHistoryTextureInput(OPAQUE_DEPTH_TEX, Stg::ComputeShader);
        data->prev_moments = rt_classify_tiles.AddHistoryTextureInput("SH Moments Tex", Stg::ComputeShader);
        data->ray_hits = rt_classify_tiles.AddStorageReadonlyInput(shadow_mask, Stg::ComputeShader);
        data->shared_data = rt_classify_tiles.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

        { // metadata buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;

            const uint32_t x_tiles = (view_state_.ren_res[0] + 8u - 1u) / 8;
            const uint32_t y_tiles = (view_state_.ren_res[1] + 4u - 1u) / 4;
            desc.size = x_tiles * y_tiles * sizeof(uint32_t);
            tiles_metadata = data->out_tile_metadata =
                rt_classify_tiles.AddStorageOutput("RT SH Tile Meta", desc, Stg::ComputeShader);
        }
        { // reprojected texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            repro_results = data->out_repro_results =
                rt_classify_tiles.AddStorageImageOutput("SH Reproj Tex", desc, Stg::ComputeShader);
        }
        { // moments texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG11F_B10F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            data->out_moments = rt_classify_tiles.AddStorageImageOutput("SH Moments Tex", desc, Stg::ComputeShader);
        }

        rt_classify_tiles.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle hist = fg.AccessROImage(data->hist);
            const Ren::ImageROHandle prev_depth = fg.AccessROImage(data->prev_depth);
            const Ren::ImageROHandle prev_moments = fg.AccessROImage(data->prev_moments);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::BufferRWHandle out_tile_metadata = fg.AccessRWBuffer(data->out_tile_metadata);
            const Ren::ImageRWHandle out_repro_results = fg.AccessRWImage(data->out_repro_results);
            const Ren::ImageRWHandle out_moments = fg.AccessRWImage(data->out_moments);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, RTShadowClassifyTiles::DEPTH_TEX_SLOT, {depth, 1}},
                {Trg::TexSampled, RTShadowClassifyTiles::VELOCITY_TEX_SLOT, velocity},
                {Trg::TexSampled, RTShadowClassifyTiles::NORM_TEX_SLOT, norm},
                {Trg::TexSampled, RTShadowClassifyTiles::HISTORY_TEX_SLOT, hist},
                {Trg::TexSampled, RTShadowClassifyTiles::PREV_DEPTH_TEX_SLOT, {prev_depth, 1}},
                {Trg::TexSampled, RTShadowClassifyTiles::PREV_MOMENTS_TEX_SLOT, prev_moments},
                {Trg::SBufRO, RTShadowClassifyTiles::RAY_HITS_BUF_SLOT, ray_hits},
                {Trg::SBufRW, RTShadowClassifyTiles::OUT_TILE_METADATA_BUF_SLOT, out_tile_metadata},
                {Trg::ImageRW, RTShadowClassifyTiles::OUT_REPROJ_RESULTS_IMG_SLOT, out_repro_results},
                {Trg::ImageRW, RTShadowClassifyTiles::OUT_MOMENTS_IMG_SLOT, out_moments},
            };

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.ren_res[0] + RTShadowClassifyTiles::GRP_SIZE_X - 1u) / RTShadowClassifyTiles::GRP_SIZE_X,
                (view_state_.ren_res[1] + RTShadowClassifyTiles::GRP_SIZE_Y - 1u) / RTShadowClassifyTiles::GRP_SIZE_Y,
                1u};

            RTShadowClassifyTiles::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.ren_res[0], view_state_.ren_res[1]);
            uniform_params.inv_img_size =
                1.0f / Ren::Vec2f{float(view_state_.ren_res[0]), float(view_state_.ren_res[1])};

            DispatchCompute(pi_shadow_classify_tiles_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle filtered_result0;

    { // Filter shadow 0
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 0");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle normal;
            FgImgROHandle input;
            FgBufROHandle tile_metadata;
            FgBufROHandle shared_data;

            FgImgRWHandle out_history;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(repro_results, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

        { // out result texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            filtered_result0 = data->out_history =
                rt_filter.AddStorageImageOutput("SH Filter 0 Tex", desc, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);
            const Ren::BufferROHandle tile_metadata = fg.AccessROBuffer(data->tile_metadata);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::ImageRWHandle out_history = fg.AccessRWImage(data->out_history);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, RTShadowFilter::DEPTH_TEX_SLOT, {depth, 1}},
                {Trg::TexSampled, RTShadowFilter::NORM_TEX_SLOT, norm},
                {Trg::TexSampled, RTShadowFilter::INPUT_TEX_SLOT, input},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, tile_metadata},
                {Trg::ImageRW, RTShadowFilter::OUT_RESULT_IMG_SLOT, out_history},
            };

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + RTShadowFilter::GRP_SIZE_X - 1u) / RTShadowFilter::GRP_SIZE_X,
                           (view_state_.ren_res[1] + RTShadowFilter::GRP_SIZE_Y - 1u) / RTShadowFilter::GRP_SIZE_Y, 1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.inv_img_size = 1.0f / Ren::Vec2f{view_state_.ren_res};

            DispatchCompute(pi_shadow_filter_[0], fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle filtered_result1;

    { // Filter shadow 1
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 1");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle normal;
            FgImgROHandle input;
            FgBufROHandle tile_metadata;
            FgBufROHandle shared_data;

            FgImgRWHandle out_history;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(filtered_result0, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

        { // out result texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            filtered_result1 = data->out_history =
                rt_filter.AddStorageImageOutput("SH Filter 1 Tex", desc, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);
            const Ren::BufferROHandle tile_metadata = fg.AccessROBuffer(data->tile_metadata);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::ImageRWHandle out_history = fg.AccessRWImage(data->out_history);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, RTShadowFilter::DEPTH_TEX_SLOT, {depth, 1}},
                {Trg::TexSampled, RTShadowFilter::NORM_TEX_SLOT, norm},
                {Trg::TexSampled, RTShadowFilter::INPUT_TEX_SLOT, input},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, tile_metadata},
                {Trg::ImageRW, RTShadowFilter::OUT_RESULT_IMG_SLOT, out_history},
            };

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + RTShadowFilter::GRP_SIZE_X - 1u) / RTShadowFilter::GRP_SIZE_X,
                           (view_state_.ren_res[1] + RTShadowFilter::GRP_SIZE_Y - 1u) / RTShadowFilter::GRP_SIZE_Y, 1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.inv_img_size = 1.0f / Ren::Vec2f{view_state_.ren_res};

            DispatchCompute(pi_shadow_filter_[1], fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle filtered_result2;

    { // Filter shadow 2
        auto &rt_filter = fg_builder_.AddNode("RT SH FILTER 2");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle normal;
            FgImgROHandle input;
            FgBufROHandle tile_metadata;
            FgBufROHandle shared_data;

            FgImgRWHandle out_history;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = rt_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->input = rt_filter.AddTextureInput(filtered_result1, Stg::ComputeShader);
        data->tile_metadata = rt_filter.AddStorageReadonlyInput(tiles_metadata, Stg::ComputeShader);
        data->shared_data = rt_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

        { // out result texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            filtered_result2 = data->out_history =
                rt_filter.AddStorageImageOutput("SH Filter 2 Tex", desc, Stg::ComputeShader);
        }

        rt_filter.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);
            const Ren::BufferROHandle tile_metadata = fg.AccessROBuffer(data->tile_metadata);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::ImageRWHandle out_history = fg.AccessRWImage(data->out_history);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, RTShadowFilter::DEPTH_TEX_SLOT, {depth, 1}},
                {Trg::TexSampled, RTShadowFilter::NORM_TEX_SLOT, norm},
                {Trg::TexSampled, RTShadowFilter::INPUT_TEX_SLOT, input},
                {Trg::SBufRO, RTShadowFilter::TILE_METADATA_BUF_SLOT, tile_metadata},
                {Trg::ImageRW, RTShadowFilter::OUT_RESULT_IMG_SLOT, out_history},
            };

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + RTShadowFilter::GRP_SIZE_X - 1u) / RTShadowFilter::GRP_SIZE_X,
                           (view_state_.ren_res[1] + RTShadowFilter::GRP_SIZE_Y - 1u) / RTShadowFilter::GRP_SIZE_Y, 1u};

            RTShadowFilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.ren_res[0], view_state_.ren_res[1]);
            uniform_params.inv_img_size = 1.0f / Ren::Vec2f{view_state_.ren_res};

            DispatchCompute(pi_shadow_filter_[2], fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgImgRWHandle shadow_final;

    { // combine shadow color with RT opaque shadow
        auto &rt_sh_combine = fg_builder_.AddNode("RT SH COMBINE");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth;
            FgImgROHandle normal;
            FgImgROHandle shadow_depth, shadow_color;
            FgImgROHandle rt_shadow;

            FgImgRWHandle out_shadow;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = rt_sh_combine.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth = rt_sh_combine.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = rt_sh_combine.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->shadow_depth = rt_sh_combine.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
        data->shadow_color = rt_sh_combine.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
        if (EnableFilter) {
            data->rt_shadow = rt_sh_combine.AddTextureInput(filtered_result2, Stg::ComputeShader);
        } else {
            data->rt_shadow = rt_sh_combine.AddTextureInput(repro_results, Stg::ComputeShader);
        }

        { // shadows texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            shadow_final = data->out_shadow =
                rt_sh_combine.AddStorageImageOutput("Sun Shadows", desc, Stg::ComputeShader);
        }

        rt_sh_combine.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
            const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);
            const Ren::ImageROHandle rt_shadow = fg.AccessROImage(data->rt_shadow);

            const Ren::ImageRWHandle out_shadow = fg.AccessRWImage(data->out_shadow);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
                {Trg::TexSampled, SunShadows::DEPTH_TEX_SLOT, {depth, 1}},
                {Trg::TexSampled, SunShadows::DEPTH_LIN_TEX_SLOT, {depth, linear_sampler_, 1}},
                {Trg::TexSampled, SunShadows::NORM_TEX_SLOT, norm},
                {Trg::TexSampled, SunShadows::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                {Trg::TexSampled, SunShadows::SHADOW_DEPTH_TEX_VAL_SLOT, {shadow_depth, nearest_sampler_}},
                {Trg::TexSampled, SunShadows::SHADOW_COLOR_TEX_SLOT, shadow_color},
                {Trg::TexSampled, SunShadows::RT_SHADOW_TEX_SLOT, rt_shadow},
                {Trg::ImageRW, SunShadows::OUT_SHADOW_IMG_SLOT, out_shadow}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + SunShadows::GRP_SIZE_X - 1u) / SunShadows::GRP_SIZE_X,
                           (view_state_.ren_res[1] + SunShadows::GRP_SIZE_Y - 1u) / SunShadows::GRP_SIZE_Y, 1u};

            SunShadows::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
            uniform_params.softness_factor =
                std::tan(p_list_->env.sun_angle * Ren::Pi<float>() / 180.0f) / 2.0f * p_list_->sun_shadow_bounds;
            uniform_params.softness_factor /= 2.0f * p_list_->sun_shadow_bounds;
            uniform_params.softness_factor *= 0.5f * float(SUN_SHADOW_RES);

            DispatchCompute(pi_sun_shadows_[1], fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    return shadow_final;
}

Eng::FgImgRWHandle Eng::Renderer::AddLQSunShadowsPass(const CommonBuffers &common_buffers,
                                                      const BindlessTextureData &bindless,
                                                      const FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    auto &sun_shadows = fg_builder_.AddNode("SUN SHADOWS");

    struct PassData {
        FgBufROHandle shared_data;
        FgImgROHandle depth;
        FgImgROHandle albedo;
        FgImgROHandle normal;
        FgImgROHandle shadow_depth, shadow_color;

        FgImgRWHandle out_shadow;
    };

    FgImgRWHandle shadow_final;

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->shared_data = sun_shadows.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
    data->depth = sun_shadows.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
    data->albedo = sun_shadows.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
    data->normal = sun_shadows.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
    data->shadow_depth = sun_shadows.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
    data->shadow_color = sun_shadows.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);

    { // shadows texture
        FgImgDesc desc;
        desc.w = view_state_.ren_res[0];
        desc.h = view_state_.ren_res[1];
        desc.format = Ren::eFormat::RGBA8;
        desc.sampling.filter = Ren::eFilter::Bilinear;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;
        shadow_final = data->out_shadow = sun_shadows.AddStorageImageOutput("Sun Shadows", desc, Stg::ComputeShader);
    }

    sun_shadows.set_execute_cb([this, data](const FgContext &fg) {
        const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
        const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
        const Ren::ImageROHandle albedo = fg.AccessROImage(data->albedo);
        const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
        const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
        const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);
        const Ren::ImageRWHandle out_shadow = fg.AccessRWImage(data->out_shadow);

        const Ren::Binding bindings[] = {
            {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
            {Trg::TexSampled, SunShadows::DEPTH_TEX_SLOT, {depth, 1}},
            {Trg::TexSampled, SunShadows::DEPTH_LIN_TEX_SLOT, {depth, linear_sampler_, 1}},
            {Trg::TexSampled, SunShadows::ALBEDO_TEX_SLOT, albedo},
            {Trg::TexSampled, SunShadows::NORM_TEX_SLOT, norm},
            {Trg::TexSampled, SunShadows::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
            {Trg::TexSampled, SunShadows::SHADOW_DEPTH_TEX_VAL_SLOT, {shadow_depth, nearest_sampler_}},
            {Trg::TexSampled, SunShadows::SHADOW_COLOR_TEX_SLOT, shadow_color},
            {Trg::ImageRW, SunShadows::OUT_SHADOW_IMG_SLOT, out_shadow}};

        const Ren::Vec3u grp_count =
            Ren::Vec3u{(view_state_.ren_res[0] + SunShadows::GRP_SIZE_X - 1u) / SunShadows::GRP_SIZE_X,
                       (view_state_.ren_res[1] + SunShadows::GRP_SIZE_Y - 1u) / SunShadows::GRP_SIZE_Y, 1u};

        SunShadows::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
        uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
        uniform_params.softness_factor =
            std::tan(p_list_->env.sun_angle * Ren::Pi<float>() / 180.0f) / 2.0f * p_list_->sun_shadow_bounds;
        uniform_params.softness_factor /= 2.0f * p_list_->sun_shadow_bounds;
        uniform_params.softness_factor *= 0.5f * float(SUN_SHADOW_RES);

        DispatchCompute(pi_sun_shadows_[0], fg.storages(), grp_count, bindings, &uniform_params, sizeof(uniform_params),
                        fg.descr_alloc(), fg.log());
    });

    return shadow_final;
}
