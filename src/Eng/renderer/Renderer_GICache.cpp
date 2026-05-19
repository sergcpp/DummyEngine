#include "Renderer.h"

#include <Ren/Context.h>

#include "executors/ExRTGICache.h"

#include "shaders/probe_blend_interface.h"
#include "shaders/probe_classify_interface.h"
#include "shaders/probe_relocate_interface.h"

void Eng::Renderer::AddGICachePasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const AccelerationStructures &acc_structs, const BindlessTextureData &bindless,
                                     const FgBufROHandle rt_geo_instances_res, const FgBufROHandle rt_obj_instances_res,
                                     FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    if (settings.gi_quality == eGIQuality::Off || settings.gi_cache_update_mode == eGICacheUpdateMode::Off) {
        return;
    }

    FgImgRWHandle ray_data;

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
        data->env = rt_gi_cache.AddTextureInput(frame_textures.envmap, stage);
        data->tlas_buf = rt_gi_cache.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);
        data->lights = rt_gi_cache.AddStorageReadonlyInput(common_buffers.lights, stage);
        data->shadow_depth = rt_gi_cache.AddTextureInput(frame_textures.shadow_depth, stage);
        data->shadow_color = rt_gi_cache.AddTextureInput(frame_textures.shadow_color, stage);
        data->ltc_luts = rt_gi_cache.AddTextureInput(frame_textures.ltc_luts, stage);
        if (common_buffers.stoch_lights) {
            data->random_seq = rt_gi_cache.AddStorageReadonlyInput(common_buffers.pmj_samples, stage);
            data->stoch_lights = rt_gi_cache.AddStorageReadonlyInput(common_buffers.stoch_lights, stage);
            data->light_nodes = rt_gi_cache.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, stage);
        }
        data->cells = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
        data->items = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_items, stage);

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = acc_structs.swrt.rt_root_node;
            data->swrt.rt_blas = rt_gi_cache.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
            data->swrt.prim_ndx = rt_gi_cache.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
            data->swrt.mesh_instances = rt_gi_cache.AddStorageReadonlyInput(rt_obj_instances_res, stage);
        }

        data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

        data->irradiance = rt_gi_cache.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
        data->distance = rt_gi_cache.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
        data->offset = rt_gi_cache.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        data->partial_update = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
        data->probe_volumes = persistent_data.probe_volumes;

        { // ~48.0mb
            FgImgDesc desc;
            desc.w = PROBE_TOTAL_RAYS_COUNT;
            desc.h = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z;
            desc.d = 3 * PROBE_VOLUME_RES_Y;
            desc.format = Ren::eFormat::RGBA16F;
            desc.flags = Ren::eImgFlags::Array;
            desc.usage = Ren::Bitmask(Ren::eImgUsage::Storage) | Ren::eImgUsage::Sampled | Ren::eImgUsage::Transfer;

            ray_data = data->out_ray_data = rt_gi_cache.AddStorageImageOutput("Probe Volume RayData", desc, stage);
        }

        rt_gi_cache.make_executor<ExRTGICache>(&view_state_, &bindless, data);
    }

    if (!ray_data) {
        return;
    }

    { // Probe blending (irradiance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND IRR");

        struct PassData {
            FgImgROHandle ray_data;
            FgImgROHandle offset;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        data->offset = probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        frame_textures.gi_cache_irradiance = data->output =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::ImageROHandle ray_data = fg.AccessROImage(data->ray_data);
            const Ren::ImageROHandle offset = fg.AccessROImage(data->offset);

            const Ren::ImageRWHandle out_irr = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::TexSampled, ProbeBlend::RAY_DATA_TEX_SLOT, ray_data},
                                             {Trg::TexSampled, ProbeBlend::OFFSET_TEX_SLOT, offset},
                                             {Trg::ImageRW, ProbeBlend::OUT_IMG_SLOT, out_irr}};

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
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }

    { // Probe blending (distance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND DIST");

        struct PassData {
            FgImgROHandle ray_data;
            FgImgROHandle offset;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        data->offset = probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        frame_textures.gi_cache_distance = data->output =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache_distance, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::ImageROHandle ray_data = fg.AccessROImage(data->ray_data);
            const Ren::ImageROHandle offset = fg.AccessROImage(data->offset);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::TexSampled, ProbeBlend::RAY_DATA_TEX_SLOT, ray_data},
                                             {Trg::TexSampled, ProbeBlend::OFFSET_TEX_SLOT, offset},
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
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }

    { // Relocate probes
        auto &probe_relocate = fg_builder_.AddNode("PROBE RELOCATE");

        struct PassData {
            FgImgROHandle ray_data;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->ray_data = probe_relocate.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output =
            probe_relocate.AddStorageImageOutput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        probe_relocate.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::ImageROHandle ray_data = fg.AccessROImage(data->ray_data);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::TexSampled, ProbeRelocate::RAY_DATA_TEX_SLOT, ray_data},
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
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
            persistent_data.probe_volumes[volume_to_update].reset_relocation = false;
        });
    }

    { // Classify probes
        auto &probe_classify = fg_builder_.AddNode("PROBE CLASSIFY");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle ray_data;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = probe_classify.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->ray_data = probe_classify.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output =
            probe_classify.AddStorageImageOutput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        probe_classify.set_execute_cb([this, data, &persistent_data](const FgContext &fg) {
            const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle ray_data = fg.AccessROImage(data->ray_data);

            const Ren::ImageRWHandle out_dist = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
                                             {Trg::TexSampled, ProbeClassify::RAY_DATA_TEX_SLOT, ray_data},
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
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
            persistent_data.probe_volumes[volume_to_update].reset_classification = false;
        });
    }
}