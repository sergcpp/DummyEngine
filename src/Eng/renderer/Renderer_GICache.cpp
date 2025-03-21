#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/probe_blend_interface.h"
#include "shaders/probe_classify_interface.h"
#include "shaders/probe_relocate_interface.h"

void Eng::Renderer::AddGICachePasses(const Ren::WeakTexRef &env_map, const CommonBuffers &common_buffers,
                                     const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, FgResRef rt_geo_instances_res,
                                     FgResRef rt_obj_instances_res, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    if (settings.gi_quality == eGIQuality::Off || settings.gi_cache_update_mode == eGICacheUpdateMode::Off) {
        return;
    }

    FgResRef ray_data;

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf && env_map) {
        auto &rt_gi_cache = fg_builder_.AddNode("RT GI CACHE");

        auto *data = rt_gi_cache.AllocNodeData<ExRTGICache::Args>();

        const auto stage = Stg::ComputeShader;

        data->geo_data = rt_gi_cache.AddStorageReadonlyInput(rt_geo_instances_res, stage);
        data->materials = rt_gi_cache.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
        data->vtx_buf1 = rt_gi_cache.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
        data->ndx_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
        data->shared_data = rt_gi_cache.AddUniformBufferInput(common_buffers.shared_data, stage);
        data->env_tex = rt_gi_cache.AddTextureInput(env_map, stage);
        data->tlas_buf = rt_gi_cache.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
        data->lights_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.lights, stage);
        data->shadow_depth_tex = rt_gi_cache.AddTextureInput(shadow_depth_tex_, stage);
        data->shadow_color_tex = rt_gi_cache.AddTextureInput(shadow_color_tex_, stage);
        data->ltc_luts_tex = rt_gi_cache.AddTextureInput(ltc_luts_, stage);
        if (persistent_data.stoch_lights_buf) {
            data->random_seq = rt_gi_cache.AddStorageReadonlyInput(pmj_samples_buf_, stage);
            data->stoch_lights_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.stoch_lights_buf, stage);
            data->light_nodes_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.stoch_lights_nodes_buf, stage);
        }
        data->cells_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
        data->items_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_items, stage);

        if (!ctx_.capabilities.hwrt) {
            data->swrt.root_node = persistent_data.swrt.rt_root_node;
            data->swrt.rt_blas_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
            data->swrt.prim_ndx_buf =
                rt_gi_cache.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
            data->swrt.mesh_instances_buf = rt_gi_cache.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(REN_GL_BACKEND)
            data->swrt.textures_buf = rt_gi_cache.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
        }

        data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

        frame_textures.gi_cache_irradiance = data->irradiance_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_irradiance, Stg::ComputeShader);
        frame_textures.gi_cache_distance = data->distance_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_distance, Stg::ComputeShader);
        frame_textures.gi_cache_offset = data->offset_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_offset, Stg::ComputeShader);

        data->view_state = &view_state_;
        data->partial_update = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
        data->probe_volumes = persistent_data.probe_volumes;

        { // ~56.6mb
            Ren::TexParams p;
            p.w = PROBE_TOTAL_RAYS_COUNT;
            p.h = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z;
            p.d = 4 * PROBE_VOLUME_RES_Y;
            p.format = Ren::eTexFormat::RGBA16F;
            p.flags = Ren::eTexFlags::Array;
            p.usage = Ren::Bitmask(Ren::eTexUsage::Storage) | Ren::eTexUsage::Sampled | Ren::eTexUsage::Transfer;

            ray_data = data->out_ray_data_tex = rt_gi_cache.AddStorageImageOutput("Probe Volume RayData", p, stage);
        }

        ex_rt_gi_cache_.Setup(fg_builder_, &view_state_, &bindless, data);
        rt_gi_cache.set_executor(&ex_rt_gi_cache_);
    }

    if (!ray_data) {
        return;
    }

    { // Probe blending (irradiance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND IRR");

        struct PassData {
            FgResRef ray_data;
            FgResRef offset_tex;
            FgResRef output_tex;
        };

        auto *data = probe_blend.AllocNodeData<PassData>();

        ray_data = data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        frame_textures.gi_cache_irradiance = data->output_tex =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
        frame_textures.gi_cache_offset = data->offset_tex =
            probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](FgBuilder &builder) {
            FgAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            FgAllocTex &offset_tex = builder.GetReadTexture(data->offset_tex);
            FgAllocTex &out_irr_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DArraySampled, ProbeBlend::RAY_DATA_TEX_SLOT, *ray_data_tex.ref},
                {Trg::Tex2DArraySampled, ProbeBlend::OFFSET_TEX_SLOT, *offset_tex.ref},
                {Trg::Image2DArray, ProbeBlend::OUT_IMG_SLOT, *out_irr_tex.ref}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            ProbeBlend::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_scroll_diff =
                Ren::Vec4i{grid_scroll_diff[0], grid_scroll_diff[1], grid_scroll_diff[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;
            uniform_params.pre_exposure = view_state_.pre_exposure;

            // total irradiance
            uniform_params.input_offset = 0;
            uniform_params.output_offset = 0;
            DispatchCompute(*pi_probe_blend_[bool(persistent_data.stoch_lights_buf)][partial],
                            Ren::Vec3u{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y}, bindings,
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());

            // diffuse-only irradiance
            uniform_params.input_offset = PROBE_VOLUME_RES_Y;
            uniform_params.output_offset = PROBE_VOLUMES_COUNT * PROBE_VOLUME_RES_Y;
            DispatchCompute(*pi_probe_blend_[bool(persistent_data.stoch_lights_buf)][partial],
                            Ren::Vec3u{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y}, bindings,
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }

    { // Probe blending (distance)
        auto &probe_blend = fg_builder_.AddNode("PROBE BLEND DIST");

        struct PassData {
            FgResRef ray_data;
            FgResRef offset_tex;
            FgResRef output_tex;
        };

        auto *data = probe_blend.AllocNodeData<PassData>();

        ray_data = data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        frame_textures.gi_cache_offset = data->offset_tex =
            probe_blend.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        frame_textures.gi_cache_distance = data->output_tex =
            probe_blend.AddStorageImageOutput(persistent_data.probe_distance, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](FgBuilder &builder) {
            FgAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            FgAllocTex &offset_tex = builder.GetReadTexture(data->offset_tex);
            FgAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DArraySampled, ProbeBlend::RAY_DATA_TEX_SLOT, *ray_data_tex.ref},
                {Trg::Tex2DArraySampled, ProbeBlend::OFFSET_TEX_SLOT, *offset_tex.ref},
                {Trg::Image2DArray, ProbeBlend::OUT_IMG_SLOT, *out_dist_tex.ref}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            ProbeBlend::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_scroll_diff =
                Ren::Vec4i{grid_scroll_diff[0], grid_scroll_diff[1], grid_scroll_diff[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;

            DispatchCompute(*pi_probe_blend_[2][partial],
                            Ren::Vec3u{PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y}, bindings,
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }

    { // Relocate probes
        auto &probe_relocate = fg_builder_.AddNode("PROBE RELOCATE");

        struct PassData {
            FgResRef ray_data;
            FgResRef output_tex;
        };

        auto *data = probe_relocate.AllocNodeData<PassData>();

        ray_data = data->ray_data = probe_relocate.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output_tex =
            probe_relocate.AddStorageImageOutput(persistent_data.probe_offset, Stg::ComputeShader);

        probe_relocate.set_execute_cb([this, data, &persistent_data](FgBuilder &builder) {
            FgAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            FgAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DArraySampled, ProbeRelocate::RAY_DATA_TEX_SLOT, *ray_data_tex.ref},
                {Trg::Image2DArray, ProbeRelocate::OUT_IMG_SLOT, *out_dist_tex.ref}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;

            const int probe_count = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z;
            const Ren::Vec3u grp_count = Ren::Vec3u{
                (probe_count + ProbeRelocate::LOCAL_GROUP_SIZE_X - 1) / ProbeRelocate::LOCAL_GROUP_SIZE_X, 1u, 1u};

            ProbeRelocate::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_scroll_diff =
                Ren::Vec4i{grid_scroll_diff[0], grid_scroll_diff[1], grid_scroll_diff[2], 0};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;

            const int pi_index =
                persistent_data.probe_volumes[volume_to_update].reset_relocation ? 2 : (partial ? 1 : 0);
            DispatchCompute(*pi_probe_relocate_[pi_index], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            ctx_.default_descr_alloc(), ctx_.log());
            persistent_data.probe_volumes[volume_to_update].reset_relocation = false;
        });
    }

    { // Classify probes
        auto &probe_classify = fg_builder_.AddNode("PROBE CLASSIFY");

        struct PassData {
            FgResRef ray_data;
            FgResRef output_tex;
        };

        auto *data = probe_classify.AllocNodeData<PassData>();

        ray_data = data->ray_data = probe_classify.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_offset = data->output_tex =
            probe_classify.AddStorageImageOutput(persistent_data.probe_offset, Stg::ComputeShader);

        probe_classify.set_execute_cb([this, data, &persistent_data](FgBuilder &builder) {
            FgAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            FgAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DArraySampled, ProbeClassify::RAY_DATA_TEX_SLOT, *ray_data_tex.ref},
                {Trg::Image2DArray, ProbeClassify::OUT_IMG_SLOT, *out_dist_tex.ref}};

            const int volume_to_update = p_list_->volume_to_update;
            const bool partial = (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial);
            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[volume_to_update].origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[volume_to_update].scroll;
            const Ren::Vec3i &grid_scroll_diff = persistent_data.probe_volumes[volume_to_update].scroll_diff;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[volume_to_update].spacing;

            const int probe_count = PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z;
            const Ren::Vec3u grp_count = Ren::Vec3u{probe_count / ProbeClassify::LOCAL_GROUP_SIZE_X, 1u, 1u};

            ProbeClassify::Params uniform_params = {};
            uniform_params.volume_index = volume_to_update;
            uniform_params.oct_index = (persistent_data.probe_volumes[volume_to_update].updates_count - 1) % 8;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_scroll_diff =
                Ren::Vec4i{grid_scroll_diff[0], grid_scroll_diff[1], grid_scroll_diff[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};
            uniform_params.quat_rot = view_state_.probe_ray_rotator;

            const int pi_index =
                persistent_data.probe_volumes[volume_to_update].reset_classification ? 2 : (partial ? 1 : 0);
            DispatchCompute(*pi_probe_classify_[pi_index], grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            ctx_.default_descr_alloc(), ctx_.log());
            persistent_data.probe_volumes[volume_to_update].reset_classification = false;
        });
    }
}