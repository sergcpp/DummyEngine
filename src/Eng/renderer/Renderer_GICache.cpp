#include "Renderer.h"

#include <Ren/Context.h>

#include "executors/ExRTGICache.h"
#include "executors/ExSampleLightsProbe.h"

#include "shaders/probe_blend_interface.h"
#include "shaders/probe_classify_interface.h"
#include "shaders/probe_relocate_interface.h"
#include "shaders/rt_gi_cache_interface.h"

void Eng::Renderer::AddGICachePasses(const PersistentGpuData &persistent_data,
                                     const AccelerationStructures &acc_structs, const BindlessTextureData &bindless,
                                     const FgBufROHandle rt_geo_instances_res, const FgBufROHandle rt_obj_instances_res,
                                     CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    if (settings.gi_quality == eGIQuality::Off || settings.gi_cache_update_mode == eGICacheUpdateMode::Off) {
        return;
    }

    FgBufRWHandle ray_hits;

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_structs.rt_tlas_buf[int(eTLASIndex::Main)] &&
        frame_textures.envmap) {
        auto &rt_gi_cache = fg_builder_.AddNode("RT GI CACHE");

        const auto stage = Stg::ComputeShader;

        auto *data = fg_builder_.AllocTempData<ExRTGICache::Args>();
        data->geo_data = rt_gi_cache.AddStorageReadonlyInput(rt_geo_instances_res, stage);
        data->materials = rt_gi_cache.AddStorageReadonlyInput(common_buffers.materials, stage);
        data->vtx_buf1 = rt_gi_cache.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
        data->ndx_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
        data->shared_data = rt_gi_cache.AddUniformBufferInput(common_buffers.shared_data, stage);
        data->tlas_buf = rt_gi_cache.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);
        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = acc_structs.swrt.rt_root_node;
            data->swrt.rt_blas = rt_gi_cache.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
            data->swrt.prim_ndx = rt_gi_cache.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
            data->swrt.mesh_instances = rt_gi_cache.AddStorageReadonlyInput(rt_obj_instances_res, stage);
        }

        data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

        data->offset = rt_gi_cache.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        data->partial_update = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
        data->probe_volumes = persistent_data.probe_volumes;

        { // ~32.0mb
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z * PROBE_TOTAL_RAYS_COUNT *
                        RTGICache::RAY_HITS_STRIDE * sizeof(uint32_t);

            ray_hits = data->out_ray_hits = rt_gi_cache.AddStorageOutput("GI Cache Ray Hits", desc, Stg::ComputeShader);
        }

        rt_gi_cache.make_executor<ExRTGICache>(&view_state_, &bindless, data);
    }

    if (!ray_hits) {
        return;
    }

    FgBufRWHandle ray_counter, ray_ids;

    { // Prepare atomic counter and ray length texture
        auto &gi_cache_prepare = fg_builder_.AddNode("RT GI CACHE PREPARE");

        struct PassData {
            FgBufRWHandle ray_counter;
            FgBufRWHandle ray_ids;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 2 * sizeof(uint32_t);

            ray_counter = data->ray_counter = gi_cache_prepare.AddTransferOutput("RT GI Cache Ray Counter", desc);
        }
        { // ray data
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = HASH_GRID_CACHE_ENTRIES_COUNT * sizeof(uint32_t);

            ray_ids = data->ray_ids = gi_cache_prepare.AddTransferOutput("RT GI Cache Ray IDs", desc);
        }

        gi_cache_prepare.set_execute_cb([data](const FgContext &fg) {
            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle ray_ids = fg.AccessRWBuffer(data->ray_ids);
            { // ray counter
                const auto &[buf_main, buf_cold] = fg.storages().buffers[ray_counter];
                Buffer_Fill(fg.ren_ctx().api(), buf_main, 0, buf_cold.size, 0, fg.cmd_buf());
            }
            { // ray data
                const auto &[buf_main, buf_cold] = fg.storages().buffers[ray_ids];
                Buffer_Fill(fg.ren_ctx().api(), buf_main, 0, buf_cold.size, 0xffffffffu, fg.cmd_buf());
            }
        });
    }

    FgBufRWHandle active_entries;

    { // Update radiance cache
        auto &rt_gi_rad_cache_update = fg_builder_.AddNode("RT GI RAD CACHE UPDATE");

        struct PassData {
            FgBufROHandle shared_data;
            FgBufROHandle ray_hits;
            FgImgROHandle offset;

            FgBufRWHandle inout_ray_counter;
            FgBufRWHandle inout_cache_entries;
            FgBufRWHandle inout_cache_voxels;
            FgBufRWHandle inout_ray_ids;
            FgBufRWHandle out_active_entries;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data =
            rt_gi_rad_cache_update.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->ray_hits = rt_gi_rad_cache_update.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
        data->offset = rt_gi_rad_cache_update.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        ray_counter = data->inout_ray_counter =
            rt_gi_rad_cache_update.AddStorageOutput(ray_counter, Stg::ComputeShader);
        ray_ids = data->inout_ray_ids = rt_gi_rad_cache_update.AddStorageOutput(ray_ids, Stg::ComputeShader);
        common_buffers.spatial_cache_entries = data->inout_cache_entries =
            rt_gi_rad_cache_update.AddStorageOutput(common_buffers.spatial_cache_entries, Stg::ComputeShader);
        common_buffers.spatial_cache_voxels = data->inout_cache_voxels =
            rt_gi_rad_cache_update.AddStorageOutput(common_buffers.spatial_cache_voxels, Stg::ComputeShader);

        { // active entries
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = sizeof(uint32_t) * std::min(HASH_GRID_CACHE_ENTRIES_COUNT,
                                                    (PROBE_TOTAL_RAYS_COUNT - PROBE_FIXED_RAYS_COUNT) *
                                                        PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z);
            active_entries = data->out_active_entries =
                rt_gi_rad_cache_update.AddTransferOutput("RT GI Cache Active Entries", desc);
        }

        rt_gi_rad_cache_update.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);
            const Ren::ImageROHandle offset = fg.AccessROImage(data->offset);

            const Ren::BufferRWHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);
            const Ren::BufferRWHandle inout_ray_ids = fg.AccessRWBuffer(data->inout_ray_ids);
            const Ren::BufferRWHandle inout_cache_entries = fg.AccessRWBuffer(data->inout_cache_entries);
            const Ren::BufferRWHandle inout_cache_voxels = fg.AccessRWBuffer(data->inout_cache_voxels);
            const Ren::BufferRWHandle out_active_entries = fg.AccessRWBuffer(data->out_active_entries);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::SBufRO, RTGICache::RAY_HITS_BUF_SLOT, ray_hits},
                                             {Trg::TexSampled, RTGICache::OFFSET_TEX_SLOT, offset},
                                             {Trg::SBufRW, RTGICache::INOUT_ENTRIES_BUF_SLOT, inout_cache_entries},
                                             {Trg::SBufRW, RTGICache::INOUT_VOXELS_BUF_SLOT, inout_cache_voxels},
                                             {Trg::SBufRW, RTGICache::RAY_COUNTER_BUF_SLOT, inout_ray_counter},
                                             {Trg::SBufRW, RTGICache::INOUT_RAY_IDS_BUF_SLOT, inout_ray_ids},
                                             {Trg::SBufRW, RTGICache::OUT_ACTIVE_BUF_SLOT, out_active_entries}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            RTGICache::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.pass_hash = view_state_.frame_index;

            const auto grp_count = Ren::Vec3u{(PROBE_TOTAL_RAYS_COUNT / RTGICache::GRP_SIZE_X),
                                              PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y};

            DispatchCompute(fg.cmd_buf(), pi_spatial_cache_update_[partial], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    { // Resolve radiance cache
        auto &rt_gi_rad_cache_resolve = fg_builder_.AddNode("RT GI RAD CACHE UPDATE");

        struct PassData {
            FgBufROHandle shared_data;

            FgBufRWHandle inout_cache_entries;
            FgBufRWHandle inout_cache_voxels;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data =
            rt_gi_rad_cache_resolve.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

        common_buffers.spatial_cache_entries = data->inout_cache_entries =
            rt_gi_rad_cache_resolve.AddStorageOutput(common_buffers.spatial_cache_entries, Stg::ComputeShader);
        common_buffers.spatial_cache_voxels = data->inout_cache_voxels =
            rt_gi_rad_cache_resolve.AddStorageOutput(common_buffers.spatial_cache_voxels, Stg::ComputeShader);

        rt_gi_rad_cache_resolve.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);

            const Ren::BufferRWHandle inout_cache_entries = fg.AccessRWBuffer(data->inout_cache_entries);
            const Ren::BufferRWHandle inout_cache_voxels = fg.AccessRWBuffer(data->inout_cache_voxels);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::SBufRW, RTGICache::INOUT_ENTRIES_BUF_SLOT, inout_cache_entries},
                                             {Trg::SBufRW, RTGICache::INOUT_VOXELS_BUF_SLOT, inout_cache_voxels}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            RTGICache::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.pass_hash = view_state_.frame_index;

            static_assert((HASH_GRID_CACHE_ENTRIES_COUNT % RTGICache::GRP_SIZE_X) == 0);
            const auto grp_count = Ren::Vec3u{HASH_GRID_CACHE_ENTRIES_COUNT / RTGICache::GRP_SIZE_X, 1u, 1u};

            DispatchCompute(fg.cmd_buf(), pi_spatial_cache_resolve_, fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgBufRWHandle indir_disp;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("RT GI CACHE INDIR ARGS");

        struct PassData {
            FgBufRWHandle ray_counter;
            FgBufRWHandle indir_disp;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        ray_counter = data->ray_counter = write_indir.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // Indirect arguments
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 3 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp = data->indir_disp =
                write_indir.AddStorageOutput("GI Cache Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle indir_args = fg.AccessRWBuffer(data->indir_disp);

            const Ren::Binding bindings[] = {{Trg::SBufRW, RTGICache::RAY_COUNTER_BUF_SLOT, ray_counter},
                                             {Trg::SBufRW, RTGICache::OUT_INDIR_ARGS_BUF_SLOT, indir_args}};

            DispatchCompute(fg.cmd_buf(), pi_cache_write_indirect_, fg.storages(), Ren::Vec3u{1u, 1u, 1u}, bindings,
                            nullptr, 0, fg.descr_alloc(), fg.log());
        });
    }

    { // Shade hit points
        auto &rt_gi_cache_shade = fg_builder_.AddNode("RT GI CACHE SHADE");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle env;
            FgBufROHandle mesh_instances;
            FgBufROHandle geo_data;
            FgBufROHandle materials;
            FgBufROHandle vtx_data0;
            FgBufROHandle ndx_buf;
            FgBufROHandle lights;
            FgBufROHandle cells, items;
            FgImgROHandle shadow_depth, shadow_color;
            FgImgROHandle ltc_luts;

            FgBufROHandle stoch_lights;
            FgBufROHandle light_nodes;

            FgImgROHandle irradiance;
            FgImgROHandle distance;
            FgImgROHandle offset;

            FgBufROHandle ray_hits;
            FgBufROHandle ray_counter;
            FgBufROHandle ray_ids;
            FgBufROHandle active_entries;
            FgBufROHandle indir_args;

            FgBufRWHandle inout_cache_voxels;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = rt_gi_cache_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->env = rt_gi_cache_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
        data->mesh_instances = rt_gi_cache_shade.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
        data->geo_data = rt_gi_cache_shade.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
        data->materials = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
        data->vtx_data0 = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
        data->ndx_buf = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
        data->lights = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);

        data->cells = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.rt_cells, Stg::ComputeShader);
        data->items = rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.rt_items, Stg::ComputeShader);

        data->shadow_depth = rt_gi_cache_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
        data->shadow_color = rt_gi_cache_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
        data->ltc_luts = rt_gi_cache_shade.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);

        if (common_buffers.stoch_lights) {
            data->stoch_lights =
                rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
            data->light_nodes =
                rt_gi_cache_shade.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
        }

        data->irradiance = rt_gi_cache_shade.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
        data->distance = rt_gi_cache_shade.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
        data->offset = rt_gi_cache_shade.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        data->ray_hits = rt_gi_cache_shade.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
        data->ray_counter = rt_gi_cache_shade.AddStorageReadonlyInput(ray_counter, Stg::ComputeShader);
        data->ray_ids = rt_gi_cache_shade.AddStorageReadonlyInput(ray_ids, Stg::ComputeShader);
        data->active_entries = rt_gi_cache_shade.AddStorageReadonlyInput(active_entries, Stg::ComputeShader);
        data->indir_args = rt_gi_cache_shade.AddIndirectBufferInput(indir_disp);

        common_buffers.spatial_cache_voxels = data->inout_cache_voxels =
            rt_gi_cache_shade.AddStorageOutput(common_buffers.spatial_cache_voxels, Stg::ComputeShader);

        rt_gi_cache_shade.set_execute_cb([this, &bindless, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle env = fg.AccessROImage(data->env);
            const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(data->mesh_instances);
            const Ren::BufferROHandle geo_data = fg.AccessROBuffer(data->geo_data);
            const Ren::BufferROHandle materials = fg.AccessROBuffer(data->materials);
            const Ren::BufferROHandle vtx_data0 = fg.AccessROBuffer(data->vtx_data0);
            const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(data->ndx_buf);
            const Ren::BufferROHandle lights = fg.AccessROBuffer(data->lights);
            const Ren::BufferROHandle cells = fg.AccessROBuffer(data->cells);
            const Ren::BufferROHandle items = fg.AccessROBuffer(data->items);
            const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
            const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);
            const Ren::ImageROHandle ltc_luts = fg.AccessROImage(data->ltc_luts);
            const Ren::ImageROHandle irr = fg.AccessROImage(data->irradiance);
            const Ren::ImageROHandle dist = fg.AccessROImage(data->distance);
            const Ren::ImageROHandle off = fg.AccessROImage(data->offset);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);
            const Ren::BufferROHandle ray_counter = fg.AccessROBuffer(data->ray_counter);
            const Ren::BufferROHandle ray_ids = fg.AccessROBuffer(data->ray_ids);
            const Ren::BufferROHandle active_entries = fg.AccessROBuffer(data->active_entries);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            Ren::BufferROHandle stoch_lights = {}, light_nodes = {};
            if (data->stoch_lights) {
                stoch_lights = fg.AccessROBuffer(data->stoch_lights);
                light_nodes = fg.AccessROBuffer(data->light_nodes);
            }

            const Ren::BufferRWHandle inout_cache_voxels = fg.AccessRWBuffer(data->inout_cache_voxels);

            Ren::SmallVector<Ren::Binding, 24> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::BindlessDescriptors, BIND_BINDLESS_TEX, bindless.rt_inline_textures},
                {Trg::TexSampled, RTGICache::ENV_TEX_SLOT, env},
                {Trg::UTBuf, RTGICache::MESH_INSTANCES_BUF_SLOT, mesh_instances},
                {Trg::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, geo_data},
                {Trg::SBufRO, RTGICache::MATERIAL_BUF_SLOT, materials},
                {Trg::UTBuf, RTGICache::VTX_BUF1_SLOT, vtx_data0},
                {Trg::UTBuf, RTGICache::NDX_BUF_SLOT, ndx_buf},
                {Trg::SBufRO, RTGICache::LIGHTS_BUF_SLOT, lights},
                {Trg::UTBuf, RTGICache::CELLS_BUF_SLOT, cells},
                {Trg::UTBuf, RTGICache::ITEMS_BUF_SLOT, items},
                {Trg::TexSampled, RTGICache::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                {Trg::TexSampled, RTGICache::SHADOW_COLOR_TEX_SLOT, shadow_color},
                {Trg::TexSampled, RTGICache::LTC_LUTS_TEX_SLOT, ltc_luts},
                {Trg::TexSampled, RTGICache::IRRADIANCE_TEX_SLOT, irr},
                {Trg::TexSampled, RTGICache::DISTANCE_TEX_SLOT, dist},
                {Trg::TexSampled, RTGICache::OFFSET_TEX_SLOT, off},
                {Trg::SBufRO, RTGICache::RAY_HITS_BUF_SLOT, ray_hits},
                {Trg::SBufRO, RTGICache::RAY_COUNTER_BUF_SLOT, ray_counter},
                {Trg::SBufRO, RTGICache::RAY_IDS_BUF_SLOT, ray_ids},
                {Trg::SBufRO, RTGICache::ACTIVE_ENTRIES_BUF_SLOT, active_entries},
                {Trg::SBufRW, RTGICache::INOUT_VOXELS_BUF_SLOT, inout_cache_voxels}};
            if (stoch_lights) {
                bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::STOCH_LIGHTS_BUF_SLOT, stoch_lights);
                bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::LIGHT_NODES_BUF_SLOT, light_nodes);
            }

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            RTGICache::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.pass_hash = ctx_.capabilities.hwrt ? 1 : 0;

            DispatchComputeIndirect(fg.cmd_buf(), pi_cache_shade_[bool(stoch_lights)][partial], fg.storages(),
                                    indir_args, 0, bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(),
                                    ctx_.log());
        });
    }

    FgBufRWHandle direct_light_data;

    { // Sample stochastic lights
        auto &sample_lights = fg_builder_.AddNode("PROBE SAMPLE LIGHTS");

        auto *data = fg_builder_.AllocTempData<ExSampleLightsProbe::Args>();
        data->shared_data = sample_lights.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->random_seq = sample_lights.AddStorageReadonlyInput(common_buffers.pmj_samples, Stg::ComputeShader);
        if (common_buffers.stoch_lights) {
            data->lights = sample_lights.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
            data->nodes = sample_lights.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
        }

        data->geo_data = sample_lights.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
        data->materials = sample_lights.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
        data->vtx_buf1 = sample_lights.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
        data->ndx_buf = sample_lights.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
        data->tlas_buf =
            sample_lights.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], Stg::ComputeShader);

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = acc_structs.swrt.rt_root_node;
            data->swrt.rt_blas_buf =
                sample_lights.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, Stg::ComputeShader);
            data->swrt.prim_ndx =
                sample_lights.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, Stg::ComputeShader);
            data->swrt.mesh_instances = sample_lights.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
        }

        data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];
        data->offset = sample_lights.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        { // ~400kb
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z * 6 * sizeof(uint32_t);

            direct_light_data = data->out_sh1_data =
                sample_lights.AddStorageOutput("GI Cache Direct SH1", desc, Stg::ComputeShader);
        }

        data->partial_update = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
        data->probe_volumes = persistent_data.probe_volumes;

        sample_lights.make_executor<ExSampleLightsProbe>(&view_state_, &bindless, data);
    }

    { // Probe blending (irradiance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND IRR");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle offset;
            FgImgROHandle env;
            FgBufROHandle lights;
            FgBufROHandle ray_hits;
            FgBufROHandle cache_entries;
            FgBufROHandle cache_voxels;
            FgBufROHandle direct_light;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = probe_blend.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->lights = probe_blend.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
        data->env = probe_blend.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
        data->offset = probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        data->ray_hits = probe_blend.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
        data->cache_entries =
            probe_blend.AddStorageReadonlyInput(common_buffers.spatial_cache_entries, Stg::ComputeShader);
        data->cache_voxels =
            probe_blend.AddStorageReadonlyInput(common_buffers.spatial_cache_voxels, Stg::ComputeShader);
        data->direct_light = probe_blend.AddStorageReadonlyInput(direct_light_data, Stg::ComputeShader);
        frame_textures.gi_cache_irradiance = data->output =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle offset = fg.AccessROImage(data->offset);
            const Ren::ImageROHandle env = fg.AccessROImage(data->env);
            const Ren::BufferROHandle lights = fg.AccessROBuffer(data->lights);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);
            const Ren::BufferROHandle cache_entries = fg.AccessROBuffer(data->cache_entries);
            const Ren::BufferROHandle cache_voxels = fg.AccessROBuffer(data->cache_voxels);
            const Ren::BufferROHandle direct_light = fg.AccessROBuffer(data->direct_light);

            const Ren::ImageRWHandle out_irr = fg.AccessRWImage(data->output);

            Ren::SmallVector<Ren::Binding, 8> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                {Trg::TexSampled, ProbeBlend::OFFSET_TEX_SLOT, offset},
                {Trg::TexSampled, ProbeBlend::ENV_TEX_SLOT, env},
                {Trg::SBufRO, ProbeBlend::LIGHTS_BUF_SLOT, lights},
                {Trg::SBufRO, ProbeBlend::RAY_HITS_BUF_SLOT, ray_hits},
                {Trg::SBufRO, ProbeBlend::CACHE_ENTRIES_BUF_SLOT, cache_entries},
                {Trg::SBufRO, ProbeBlend::CACHE_VOXELS_BUF_SLOT, cache_voxels},
                {Trg::ImageRW, ProbeBlend::OUT_IMG_SLOT, out_irr}};
            if (persistent_data.stoch_lights) {
                bindings.emplace_back(Trg::SBufRO, ProbeBlend::DIRECT_LIGHT_BUF_SLOT, direct_light);
            }

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            ProbeBlend::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.pre_exposure = view_state_.pre_exposure;

            DispatchCompute(fg.cmd_buf(), pi_probe_blend_[bool(persistent_data.stoch_lights)][partial], fg.storages(),
                            Ren::Vec3u{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y}, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    { // Probe blending (distance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND DIST");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle offset;
            FgBufROHandle ray_hits;
            FgBufROHandle cache_entries;
            FgBufROHandle cache_voxels;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = probe_blend.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->offset = probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        data->ray_hits = probe_blend.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
        data->cache_entries =
            probe_blend.AddStorageReadonlyInput(common_buffers.spatial_cache_entries, Stg::ComputeShader);
        data->cache_voxels =
            probe_blend.AddStorageReadonlyInput(common_buffers.spatial_cache_voxels, Stg::ComputeShader);

        frame_textures.gi_cache_distance = data->output =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache_distance, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle offset = fg.AccessROImage(data->offset);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);
            const Ren::BufferROHandle cache_entries = fg.AccessROBuffer(data->cache_entries);
            const Ren::BufferROHandle cache_voxels = fg.AccessROBuffer(data->cache_voxels);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::TexSampled, ProbeBlend::OFFSET_TEX_SLOT, offset},
                                             {Trg::SBufRO, ProbeBlend::RAY_HITS_BUF_SLOT, ray_hits},
                                             {Trg::SBufRO, ProbeBlend::CACHE_ENTRIES_BUF_SLOT, cache_entries},
                                             {Trg::SBufRO, ProbeBlend::CACHE_VOXELS_BUF_SLOT, cache_voxels},
                                             {Trg::ImageRW, ProbeBlend::OUT_IMG_SLOT, out_dist}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            ProbeBlend::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;

            DispatchCompute(fg.cmd_buf(), pi_probe_blend_[2][partial], fg.storages(),
                            Ren::Vec3u{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y}, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    { // Relocate probes
        auto &probe_relocate = fg_builder_.AddNode("PROBE RELOCATE");

        struct PassData {
            FgBufROHandle ray_hits;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->ray_hits = probe_relocate.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output =
            probe_relocate.AddStorageImageOutput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        probe_relocate.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::SBufRO, ProbeRelocate::RAY_HITS_BUF_SLOT, ray_hits},
                                             {Trg::ImageRW, ProbeRelocate::OUT_IMG_SLOT, out_dist}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            const int probe_count = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z;
            const Ren::Vec3u grp_count =
                Ren::Vec3u{(probe_count + ProbeRelocate::GRP_SIZE_X - 1) / ProbeRelocate::GRP_SIZE_X, 1u, 1u};

            ProbeRelocate::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;

            const int pi_index =
                persistent_data.probe_volumes[volume_to_update].reset_relocation ? 2 : (partial ? 1 : 0);
            DispatchCompute(fg.cmd_buf(), pi_probe_relocate_[pi_index], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            persistent_data.probe_volumes[volume_to_update].reset_relocation = false;
        });
    }

    { // Classify probes
        auto &probe_classify = fg_builder_.AddNode("PROBE CLASSIFY");

        struct PassData {
            FgBufROHandle shared_data;
            FgBufROHandle ray_hits;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = probe_classify.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->ray_hits = probe_classify.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output =
            probe_classify.AddStorageImageOutput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        probe_classify.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::BufferROHandle ray_hits = fg.AccessROBuffer(data->ray_hits);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
                                             {Trg::SBufRO, ProbeClassify::RAY_HITS_BUF_SLOT, ray_hits},
                                             {Trg::ImageRW, ProbeClassify::OUT_IMG_SLOT, out_dist}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;

            const int probe_count = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z;
            const Ren::Vec3u grp_count = Ren::Vec3u{probe_count / ProbeClassify::GRP_SIZE_X, 1u, 1u};

            ProbeClassify::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin, 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll, 0};
            uniform_params.grid_scroll_diff = Ren::Vec4i{grid_scroll_diff, 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing, Length(grid_spacing)};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.vol_bbox_min = Ren::Vec4f{p_list_->env.fog.bbox_min};
            uniform_params.vol_bbox_max = Ren::Vec4f{p_list_->env.fog.bbox_max};

            int pi_index = 4;
            if (p_list_->env.fog.density > 0.0f) {
                pi_index = (partial ? 3 : 2);
            } else {
                pi_index = (partial ? 1 : 0);
            }
            if (persistent_data.probe_volumes[volume_to_update].reset_classification) {
                pi_index = 4;
            }

            DispatchCompute(fg.cmd_buf(), pi_probe_classify_[pi_index], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            persistent_data.probe_volumes[volume_to_update].reset_classification = false;
        });
    }
}