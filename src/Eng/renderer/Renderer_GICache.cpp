#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/probe_blend_interface.h"
#include "shaders/probe_classify_interface.h"
#include "shaders/probe_relocate_interface.h"

void Eng::Renderer::AddGICachePasses(const Ren::WeakTex2DRef &env_map, const CommonBuffers &common_buffers,
                                     const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, RpResRef rt_obj_instances_res,
                                     FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    RpResRef ray_data;

    if ((ctx_.capabilities.raytracing || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf && env_map) {
        auto &rt_gi_cache = rp_builder_.AddPass("RT GI CACHE");

        auto *data = rt_gi_cache.AllocPassData<RpRTGICacheData>();

        const auto stage = ctx_.capabilities.ray_query
                               ? Stg::ComputeShader
                               : (ctx_.capabilities.raytracing ? Stg::RayTracingShader : Stg::ComputeShader);

        data->geo_data = rt_gi_cache.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stage);
        data->materials = rt_gi_cache.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
        data->vtx_buf1 = rt_gi_cache.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
        data->vtx_buf2 = rt_gi_cache.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
        data->ndx_buf = rt_gi_cache.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
        data->shared_data = rt_gi_cache.AddUniformBufferInput(common_buffers.shared_data_res, stage);
        data->env_tex = rt_gi_cache.AddTextureInput(env_map, stage);
        data->tlas_buf = rt_gi_cache.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
        data->lights_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.lights_res, stage);
        data->shadowmap_tex = rt_gi_cache.AddTextureInput(shadow_map_tex_, stage);
        data->ltc_luts_tex = rt_gi_cache.AddTextureInput(ltc_luts_, stage);
        data->cells_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_cells_res, stage);
        data->items_buf = rt_gi_cache.AddStorageReadonlyInput(common_buffers.rt_items_res, stage);

        if (!ctx_.capabilities.raytracing) {
            data->swrt.root_node = persistent_data.swrt.rt_root_node;
            data->swrt.rt_blas_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.rt_blas_buf, stage);
            data->swrt.prim_ndx_buf =
                rt_gi_cache.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
            data->swrt.meshes_buf = rt_gi_cache.AddStorageReadonlyInput(persistent_data.swrt.rt_meshes_buf, stage);
            data->swrt.mesh_instances_buf = rt_gi_cache.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(USE_GL_RENDER)
            data->swrt.textures_buf = rt_gi_cache.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
        }

        data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

        frame_textures.gi_cache = data->irradiance_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_volume.irradiance.get(), Stg::ComputeShader);
        frame_textures.gi_cache_dist = data->distance_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_volume.distance.get(), Stg::ComputeShader);
        frame_textures.gi_cache_data = data->offset_tex =
            rt_gi_cache.AddTextureInput(persistent_data.probe_volume.data.get(), Stg::ComputeShader);

        data->grid_origin = persistent_data.probe_volume.origin;
        data->grid_scroll = persistent_data.probe_volume.scroll;
        data->grid_spacing = persistent_data.probe_volume.spacing;

        ray_data = data->out_ray_data_tex =
            rt_gi_cache.AddStorageImageOutput(persistent_data.probe_volume.ray_data.get(), stage);

        rp_rt_gi_cache_.Setup(rp_builder_, &view_state_, &bindless, data);
        rt_gi_cache.set_executor(&rp_rt_gi_cache_);
    }

    if (!ray_data) {
        return;
    }

    { // Probe blending (irradiance)
        auto &probe_blend = rp_builder_.AddPass("PROBE BLEND IRR");

        struct PassData {
            RpResRef ray_data;
            RpResRef offset_tex;
            RpResRef output_tex;
        };

        auto *data = probe_blend.AllocPassData<PassData>();

        ray_data = data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        frame_textures.gi_cache_data = data->offset_tex =
            probe_blend.AddTextureInput(frame_textures.gi_cache_data, Stg::ComputeShader);

        frame_textures.gi_cache = data->output_tex =
            probe_blend.AddStorageImageOutput(frame_textures.gi_cache, Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](RpBuilder &builder) {
            RpAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            RpAllocTex &offset_tex = builder.GetReadTexture(data->offset_tex);
            RpAllocTex &out_irr_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DArray, ProbeBlend::RAY_DATA_TEX_SLOT, *ray_data_tex.arr},
                                             {Trg::Tex2DArray, ProbeBlend::OFFSET_TEX_SLOT, *offset_tex.arr},
                                             {Trg::Image2DArray, ProbeBlend::OUT_IMG_SLOT, *out_irr_tex.arr}};

            const Ren::Vec3f &grid_origin = persistent_data.probe_volume.origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volume.scroll;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volume.spacing;

            ProbeBlend::Params uniform_params = {};
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};

            Ren::DispatchCompute(pi_probe_blend_[0], Ren::Vec3u{PROBE_VOLUME_RES, PROBE_VOLUME_RES, PROBE_VOLUME_RES},
                                 bindings, &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(),
                                 ctx_.log());
        });
    }

    { // Probe blending (distance)
        auto &probe_blend = rp_builder_.AddPass("PROBE BLEND DIST");

        struct PassData {
            RpResRef ray_data;
            RpResRef offset_tex;
            RpResRef output_tex;
        };

        auto *data = probe_blend.AllocPassData<PassData>();

        ray_data = data->ray_data = probe_blend.AddTextureInput(ray_data, Stg::ComputeShader);
        frame_textures.gi_cache_data = data->offset_tex =
            probe_blend.AddTextureInput(frame_textures.gi_cache_data, Stg::ComputeShader);

        frame_textures.gi_cache_dist = data->output_tex =
            probe_blend.AddStorageImageOutput(persistent_data.probe_volume.distance.get(), Stg::ComputeShader);

        probe_blend.set_execute_cb([this, data, &persistent_data](RpBuilder &builder) {
            RpAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            RpAllocTex &offset_tex = builder.GetReadTexture(data->offset_tex);
            RpAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DArray, ProbeBlend::RAY_DATA_TEX_SLOT, *ray_data_tex.arr},
                                             {Trg::Tex2DArray, ProbeBlend::OFFSET_TEX_SLOT, *offset_tex.arr},
                                             {Trg::Image2DArray, ProbeBlend::OUT_IMG_SLOT, *out_dist_tex.arr}};

            const Ren::Vec3f &grid_origin = persistent_data.probe_volume.origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volume.scroll;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volume.spacing;

            ProbeBlend::Params uniform_params = {};
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};

            Ren::DispatchCompute(pi_probe_blend_[1], Ren::Vec3u{PROBE_VOLUME_RES, PROBE_VOLUME_RES, PROBE_VOLUME_RES},
                                 bindings, &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(),
                                 ctx_.log());
        });
    }

    { // Relocate probes
        auto &probe_relocate = rp_builder_.AddPass("PROBE RELOC");

        struct PassData {
            RpResRef ray_data;
            RpResRef output_tex;
        };

        auto *data = probe_relocate.AllocPassData<PassData>();

        ray_data = data->ray_data = probe_relocate.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_data = data->output_tex =
            probe_relocate.AddStorageImageOutput(persistent_data.probe_volume.data.get(), Stg::ComputeShader);

        probe_relocate.set_execute_cb([this, data, &persistent_data](RpBuilder &builder) {
            RpAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            RpAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DArray, ProbeRelocate::RAY_DATA_TEX_SLOT, *ray_data_tex.arr},
                                             {Trg::Image2DArray, ProbeRelocate::OUT_IMG_SLOT, *out_dist_tex.arr}};

            const Ren::Vec3f &grid_origin = persistent_data.probe_volume.origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volume.scroll;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volume.spacing;

            ProbeRelocate::Params uniform_params = {};
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};

            const int probe_count = PROBE_VOLUME_RES * PROBE_VOLUME_RES * PROBE_VOLUME_RES;
            const Ren::Vec3u grp_count = Ren::Vec3u{
                (probe_count + ProbeRelocate::LOCAL_GROUP_SIZE_X - 1) / ProbeRelocate::LOCAL_GROUP_SIZE_X, 1u, 1u};

            int pi_index = 0;
            if (persistent_data.probe_volume.reset_relocation) {
                pi_index = 1;
                persistent_data.probe_volume.reset_relocation = false;
            }

            Ren::DispatchCompute(pi_probe_relocate_[pi_index], grp_count, bindings, &uniform_params,
                                 sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }

    { // Classify probes
        auto &probe_classify = rp_builder_.AddPass("PROBE CLASSIFY");

        struct PassData {
            RpResRef ray_data;
            RpResRef output_tex;
        };

        auto *data = probe_classify.AllocPassData<PassData>();

        ray_data = data->ray_data = probe_classify.AddTextureInput(ray_data, Stg::ComputeShader);

        frame_textures.gi_cache_data = data->output_tex =
            probe_classify.AddStorageImageOutput(persistent_data.probe_volume.data.get(), Stg::ComputeShader);

        probe_classify.set_execute_cb([this, data, &persistent_data](RpBuilder &builder) {
            RpAllocTex &ray_data_tex = builder.GetReadTexture(data->ray_data);
            RpAllocTex &out_dist_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DArray, ProbeClassify::RAY_DATA_TEX_SLOT, *ray_data_tex.arr},
                                             {Trg::Image2DArray, ProbeClassify::OUT_IMG_SLOT, *out_dist_tex.arr}};

            const Ren::Vec3f &grid_origin = persistent_data.probe_volume.origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volume.scroll;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volume.spacing;

            ProbeClassify::Params uniform_params = {};
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};

            const int probe_count = PROBE_VOLUME_RES * PROBE_VOLUME_RES * PROBE_VOLUME_RES;
            const Ren::Vec3u grp_count = Ren::Vec3u{
                (probe_count + ProbeClassify::LOCAL_GROUP_SIZE_X - 1) / ProbeClassify::LOCAL_GROUP_SIZE_X, 1u, 1u};

            int pi_index = 0;
            if (persistent_data.probe_volume.reset_classification) {
                pi_index = 1;
                persistent_data.probe_volume.reset_classification = false;
            }

            Ren::DispatchCompute(pi_probe_classify_[pi_index], grp_count, bindings, &uniform_params,
                                 sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
}