#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"
#include "executors/ExOITBlendLayer.h"
#include "executors/ExOITDepthPeel.h"
#include "executors/ExOITScheduleRays.h"

#include "shaders/ssr_trace_hq_interface.h"
#include "shaders/ssr_write_indir_rt_dispatch_interface.h"
#include "shaders/ssr_write_indirect_args_interface.h"

void Eng::Renderer::AddOITPasses(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                 const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                                 const FgResRef depth_hierarchy, FgResRef rt_geo_instances_res,
                                 FgResRef rt_obj_instances_res, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    FgResRef oit_depth_buf, oit_specular[OIT_REFLECTION_LAYERS], oit_rays_counter;

    const int oit_layer_count =
        (settings.transparency_quality == eTransparencyQuality::Ultra) ? OIT_LAYERS_ULTRA : OIT_LAYERS_HIGH;

    { // OIT clear
        auto &oit_clear = fg_builder_.AddNode("OIT CLEAR");

        struct PassData {
            FgResRef oit_depth_buf;
            FgResRef oit_specular[OIT_REFLECTION_LAYERS];
            FgResRef oit_rays_counter;
        };

        auto *data = oit_clear.AllocNodeData<PassData>();

        { // output buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Texture;
            desc.size = oit_layer_count * view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);
            desc.views.push_back(Ren::eTexFormat::R32UI);
            oit_depth_buf = data->oit_depth_buf = oit_clear.AddTransferOutput("Depth Values", desc);
        }

        // specular buffers (halfres)
        for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
            const std::string tex_name = "OIT REFL #" + std::to_string(i);

            Ren::TexParams params;
            params.w = (view_state_.scr_res[0] / 2);
            params.h = (view_state_.scr_res[1] / 2);
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            oit_specular[i] = data->oit_specular[i] = oit_clear.AddTransferImageOutput(tex_name, params);
        }

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 8 * sizeof(uint32_t);

            oit_rays_counter = data->oit_rays_counter = oit_clear.AddTransferOutput("OIT Ray Counter", desc);
        }

        oit_clear.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocBuf &out_depth_buf = builder.GetWriteBuffer(data->oit_depth_buf);
            FgAllocBuf &out_rays_counter_buf = builder.GetWriteBuffer(data->oit_rays_counter);

            Ren::Context &ctx = builder.ctx();
            out_rays_counter_buf.ref->Fill(0, out_rays_counter_buf.ref->size(), 0, ctx.current_cmd_buf());

            if (p_list_->alpha_blend_start_index != -1) {
                out_depth_buf.ref->Fill(0, out_depth_buf.ref->size(), 0, ctx.current_cmd_buf());
            }
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                FgAllocTex &oit_specular = builder.GetWriteTexture(data->oit_specular[i]);
                if (p_list_->alpha_blend_start_index != -1) {
                    const float rgba[4] = {};
                    Ren::ClearImage(*oit_specular.ref, rgba, ctx.current_cmd_buf());
                }
            }
        });
    }

    { // depth peel pass
        auto &oit_depth_peel = fg_builder_.AddNode("OIT DEPTH PEEL");
        const FgResRef vtx_buf1 = oit_depth_peel.AddVertexBufferInput(persistent_data.vertex_buf1);
        const FgResRef vtx_buf2 = oit_depth_peel.AddVertexBufferInput(persistent_data.vertex_buf2);
        const FgResRef ndx_buf = oit_depth_peel.AddIndexBufferInput(persistent_data.indices_buf);

        const FgResRef materials_buf =
            oit_depth_peel.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
        const FgResRef textures_buf = oit_depth_peel.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
        const FgResRef textures_buf = {};
#endif

        [[maybe_unused]] const FgResRef noise_tex =
            oit_depth_peel.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
        const FgResRef white_tex =
            oit_depth_peel.AddTextureInput(dummy_white_, Stg::VertexShader | Stg::FragmentShader);
        const FgResRef instances_buf =
            oit_depth_peel.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
        const FgResRef instances_indices_buf =
            oit_depth_peel.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

        const FgResRef shader_data_buf =
            oit_depth_peel.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

        frame_textures.depth = oit_depth_peel.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
        oit_depth_buf = oit_depth_peel.AddStorageOutput(oit_depth_buf, Stg::FragmentShader);

        oit_depth_peel.make_executor<ExOITDepthPeel>(
            &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless, white_tex,
            instances_buf, instances_indices_buf, shader_data_buf, frame_textures.depth, oit_depth_buf);
    }

    FgResRef oit_ray_list;

    { // schedule reflection rays
        auto &oit_schedule = fg_builder_.AddNode("OIT SCHEDULE");

        const FgResRef vtx_buf1 = oit_schedule.AddVertexBufferInput(persistent_data.vertex_buf1);
        const FgResRef vtx_buf2 = oit_schedule.AddVertexBufferInput(persistent_data.vertex_buf2);
        const FgResRef ndx_buf = oit_schedule.AddIndexBufferInput(persistent_data.indices_buf);

        const FgResRef materials_buf =
            oit_schedule.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
        const FgResRef textures_buf = oit_schedule.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
        const FgResRef textures_buf = {};
#endif

        [[maybe_unused]] const FgResRef noise_tex =
            oit_schedule.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
        const FgResRef white_tex = oit_schedule.AddTextureInput(dummy_white_, Stg::VertexShader | Stg::FragmentShader);
        const FgResRef instances_buf =
            oit_schedule.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
        const FgResRef instances_indices_buf =
            oit_schedule.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

        const FgResRef shader_data_buf =
            oit_schedule.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

        frame_textures.depth = oit_schedule.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
        oit_depth_buf = oit_schedule.AddStorageReadonlyInput(oit_depth_buf, Stg::FragmentShader);

        oit_rays_counter = oit_schedule.AddStorageOutput(oit_rays_counter, Stg::FragmentShader);
        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = OIT_REFLECTION_LAYERS * (view_state_.scr_res[0] / 2) * (view_state_.scr_res[1] / 2) * 2 *
                        sizeof(uint32_t);

            oit_ray_list = oit_schedule.AddStorageOutput("OIT Ray List", desc, Stg::FragmentShader);
        }

        oit_schedule.make_executor<ExOITScheduleRays>(
            &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless, noise_tex,
            white_tex, instances_buf, instances_indices_buf, shader_data_buf, frame_textures.depth, oit_depth_buf,
            oit_rays_counter, oit_ray_list);
    }

    FgResRef indir_disp_buf;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("OIT INDIR ARGS");

        struct PassData {
            FgResRef ray_counter;
            FgResRef indir_disp_buf;
        };

        auto *data = write_indir.AllocNodeData<PassData>();
        oit_rays_counter = data->ray_counter = write_indir.AddStorageOutput(oit_rays_counter, Stg::ComputeShader);

        { // Indirect arguments
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 3 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp_buf = data->indir_disp_buf =
                write_indir.AddStorageOutput("OIT Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](FgBuilder &builder) {
            using namespace SSRWriteIndirectArgs;

            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            FgAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                             {Trg::SBufRW, INDIR_ARGS_SLOT, *indir_args.ref}};

            DispatchCompute(*pi_ssr_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef ray_rt_list;

    if (settings.reflections_quality != eReflectionsQuality::Raytraced_High) {
        auto &ssr_trace_hq = fg_builder_.AddNode("OIT SSR");

        struct PassData {
            FgResRef oit_depth_buf;
            FgResRef shared_data;
            FgResRef color_tex, normal_tex, depth_hierarchy;

            FgResRef albedo_tex, specular_tex, ltc_luts_tex, irradiance_tex, distance_tex, offset_tex;

            FgResRef in_ray_list, indir_args, inout_ray_counter;
            FgResRef out_ssr_tex[OIT_REFLECTION_LAYERS], out_ray_list;
        };

        auto *data = ssr_trace_hq.AllocNodeData<PassData>();
        data->oit_depth_buf = ssr_trace_hq.AddStorageReadonlyInput(oit_depth_buf, Stg::ComputeShader);
        data->shared_data = ssr_trace_hq.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->color_tex = ssr_trace_hq.AddTextureInput(frame_textures.color, Stg::ComputeShader);
        data->normal_tex = ssr_trace_hq.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->depth_hierarchy = ssr_trace_hq.AddTextureInput(depth_hierarchy, Stg::ComputeShader);

        if (frame_textures.gi_cache_irradiance) {
            data->albedo_tex = ssr_trace_hq.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
            data->specular_tex = ssr_trace_hq.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->ltc_luts_tex = ssr_trace_hq.AddTextureInput(ltc_luts_, Stg::ComputeShader);
            data->irradiance_tex = ssr_trace_hq.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance_tex = ssr_trace_hq.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset_tex = ssr_trace_hq.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        }

        data->in_ray_list = ssr_trace_hq.AddStorageReadonlyInput(oit_ray_list, Stg::ComputeShader);
        data->indir_args = ssr_trace_hq.AddIndirectBufferInput(indir_disp_buf);
        oit_rays_counter = data->inout_ray_counter =
            ssr_trace_hq.AddStorageOutput(oit_rays_counter, Stg::ComputeShader);
        for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
            oit_specular[i] = data->out_ssr_tex[i] =
                ssr_trace_hq.AddStorageImageOutput(oit_specular[i], Stg::ComputeShader);
        }

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = OIT_REFLECTION_LAYERS * (view_state_.scr_res[0] / 2) * (view_state_.scr_res[1] / 2) * 2 *
                        sizeof(uint32_t);

            ray_rt_list = data->out_ray_list =
                ssr_trace_hq.AddStorageOutput("OIT RT Ray List", desc, Stg::ComputeShader);
        }

        ssr_trace_hq.set_execute_cb([this, data](FgBuilder &builder) {
            using namespace SSRTraceHQ;

            FgAllocBuf &oit_depth_buf = builder.GetReadBuffer(data->oit_depth_buf);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &color_tex = builder.GetReadTexture(data->color_tex);
            FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            FgAllocTex &depth_hierarchy_tex = builder.GetReadTexture(data->depth_hierarchy);
            FgAllocBuf &in_ray_list_buf = builder.GetReadBuffer(data->in_ray_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex *albedo_tex = nullptr, *specular_tex = nullptr, *ltc_luts_tex = nullptr, *irr_tex = nullptr,
                       *dist_tex = nullptr, *off_tex = nullptr;
            if (data->irradiance_tex) {
                albedo_tex = &builder.GetReadTexture(data->albedo_tex);
                specular_tex = &builder.GetReadTexture(data->specular_tex);
                ltc_luts_tex = &builder.GetReadTexture(data->ltc_luts_tex);

                irr_tex = &builder.GetReadTexture(data->irradiance_tex);
                dist_tex = &builder.GetReadTexture(data->distance_tex);
                off_tex = &builder.GetReadTexture(data->offset_tex);
            }

            FgAllocTex *out_refl_tex[OIT_REFLECTION_LAYERS] = {};
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                out_refl_tex[i] = &builder.GetWriteTexture(data->out_ssr_tex[i]);
            }
            FgAllocBuf &inout_ray_counter_buf = builder.GetWriteBuffer(data->inout_ray_counter);
            FgAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(data->out_ray_list);

            Ren::SmallVector<Ren::Binding, 24> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                {Trg::Tex2DSampled, COLOR_TEX_SLOT, *color_tex.ref},
                {Trg::Tex2DSampled, NORM_TEX_SLOT, *normal_tex.ref},
                {Trg::UTBuf, OIT_DEPTH_BUF_SLOT, *oit_depth_buf.ref},
                {Trg::SBufRO, IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                {Trg::SBufRW, OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                bindings.emplace_back(Trg::Image, OUT_REFL_IMG_SLOT, i, 1, *out_refl_tex[i]->ref);
            }
            if (irr_tex) {
                bindings.emplace_back(Trg::Tex2DSampled, ALBEDO_TEX_SLOT, *albedo_tex->ref);
                bindings.emplace_back(Trg::Tex2DSampled, SPEC_TEX_SLOT, *specular_tex->ref);
                bindings.emplace_back(Trg::Tex2DSampled, LTC_LUTS_TEX_SLOT, *ltc_luts_tex->ref);

                bindings.emplace_back(Trg::Tex2DArraySampled, IRRADIANCE_TEX_SLOT, *irr_tex->ref);
                bindings.emplace_back(Trg::Tex2DArraySampled, DISTANCE_TEX_SLOT, *dist_tex->ref);
                bindings.emplace_back(Trg::Tex2DArraySampled, OFFSET_TEX_SLOT, *off_tex->ref);
            }

            Params uniform_params;
            uniform_params.resolution = Ren::Vec4u(view_state_.act_res[0], view_state_.act_res[1], 0, 0);

            DispatchComputeIndirect(*pi_ssr_trace_hq_[1][irr_tex != nullptr], *indir_args_buf.ref, 0, bindings,
                                    &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                    builder.ctx().log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf &&
        int(settings.reflections_quality) >= int(eReflectionsQuality::Raytraced_Normal)) {
        FgResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("OIT RT ARGS");

            struct PassData {
                FgResRef ray_counter;
                FgResRef indir_disp_buf;
            };

            auto *data = rt_disp_args.AllocNodeData<PassData>();
            oit_rays_counter = data->ray_counter = rt_disp_args.AddStorageOutput(oit_rays_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp_buf = data->indir_disp_buf =
                    rt_disp_args.AddStorageOutput("OIT RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](FgBuilder &builder) {
                using namespace SSRWriteIndirRTDispatch;

                FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                FgAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                Params params = {};
                params.counter_index = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 1 : 6;

                DispatchCompute(*pi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, &params, sizeof(params),
                                builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace reflection rays
            auto &rt_refl = fg_builder_.AddNode("OIT RT REFL");

            auto *data = rt_refl.AllocNodeData<ExRTReflections::Args>();

            const auto stage = Stg::ComputeShader;

            data->geo_data = rt_refl.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_refl.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_refl.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
            data->vtx_buf2 = rt_refl.AddStorageReadonlyInput(persistent_data.vertex_buf2, stage);
            data->ndx_buf = rt_refl.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
            data->shared_data = rt_refl.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->depth_tex = rt_refl.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_refl.AddTextureInput(frame_textures.normal, stage);
            data->env_tex = rt_refl.AddTextureInput(frame_textures.envmap, stage);
            data->ray_counter = rt_refl.AddStorageReadonlyInput(oit_rays_counter, stage);
            if (ray_rt_list) {
                data->ray_list = rt_refl.AddStorageReadonlyInput(ray_rt_list, stage);
            } else {
                data->ray_list = rt_refl.AddStorageReadonlyInput(oit_ray_list, stage);
            }
            data->indir_args = rt_refl.AddIndirectBufferInput(indir_rt_disp_buf);
            data->tlas_buf = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
            data->lights_buf = rt_refl.AddStorageReadonlyInput(common_buffers.lights, stage);
            data->shadow_depth_tex = rt_refl.AddTextureInput(shadow_depth_tex_, stage);
            data->shadow_color_tex = rt_refl.AddTextureInput(shadow_color_tex_, stage);
            data->ltc_luts_tex = rt_refl.AddTextureInput(ltc_luts_, stage);
            data->cells_buf = rt_refl.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
            data->items_buf = rt_refl.AddStorageReadonlyInput(common_buffers.rt_items, stage);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx_buf =
                    rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
                data->swrt.mesh_instances_buf = rt_refl.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(REN_GL_BACKEND)
                data->swrt.textures_buf = rt_refl.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
            }

            data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

            data->probe_volume = &persistent_data.probe_volumes[0];
            if (settings.gi_quality != eGIQuality::Off) {
                data->irradiance_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
                data->distance_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_distance, stage);
                data->offset_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_offset, stage);
            }

            oit_depth_buf = data->oit_depth_buf = rt_refl.AddStorageReadonlyInput(oit_depth_buf, stage);

            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                oit_specular[i] = data->out_refl_tex[i] = rt_refl.AddStorageImageOutput(oit_specular[i], stage);
            }

            ex_oit_rt_reflections_.Setup(fg_builder_, &view_state_, &bindless, data);
            rt_refl.set_executor(&ex_oit_rt_reflections_);
        }
    }

    for (int i = 0; i < oit_layer_count; ++i) {
        FgResRef back_color, back_depth;
        { // copy background
            const std::string pass_name = "OIT BACK #" + std::to_string(i);

            auto &oit_back = fg_builder_.AddNode(pass_name);

            struct PassData {
                FgResRef in_back_color;
                FgResRef in_back_depth;
                FgResRef out_back_color;
                FgResRef out_back_depth;
            };

            auto *data = oit_back.AllocNodeData<PassData>();
            data->in_back_color = oit_back.AddTransferImageInput(frame_textures.color);
            data->in_back_depth = oit_back.AddTransferImageInput(frame_textures.depth);
            { // background color
                const std::string tex_name = "OIT Back Color #" + std::to_string(i);
                back_color = data->out_back_color =
                    oit_back.AddTransferImageOutput(tex_name, frame_textures.color_params);
            }
            { // background depth
                const std::string tex_name = i == 0 ? OPAQUE_DEPTH_TEX : "OIT Back Depth #" + std::to_string(i);
                back_depth = data->out_back_depth =
                    oit_back.AddTransferImageOutput(tex_name, frame_textures.depth_params);
                if (i == 0) {
                    frame_textures.opaque_depth = back_depth;
                }
            }

            oit_back.set_execute_cb([this, data, i](FgBuilder &builder) {
                FgAllocTex &in_back_color_tex = builder.GetReadTexture(data->in_back_color);
                FgAllocTex &in_back_depth_tex = builder.GetReadTexture(data->in_back_depth);
                FgAllocTex &out_back_color_tex = builder.GetWriteTexture(data->out_back_color);
                FgAllocTex &out_back_depth_tex = builder.GetWriteTexture(data->out_back_depth);

                const int w = in_back_color_tex.ref->params.w, h = in_back_color_tex.ref->params.h;

                if (p_list_->alpha_blend_start_index != -1) {
                    CopyImageToImage(builder.ctx().current_cmd_buf(), *in_back_color_tex.ref, 0, 0, 0, 0,
                                     *out_back_color_tex.ref, 0, 0, 0, 0, 0, w, h, 1);
                }
                if (i == 0 || p_list_->alpha_blend_start_index != -1) {
                    CopyImageToImage(builder.ctx().current_cmd_buf(), *in_back_depth_tex.ref, 0, 0, 0, 0,
                                     *out_back_depth_tex.ref, 0, 0, 0, 0, 0, w, h, 1);
                }
            });
        }
        { // blend
            const std::string pass_name = "OIT BLEND #" + std::to_string(i);

            auto &oit_blend_layer = fg_builder_.AddNode(pass_name);
            const FgResRef vtx_buf1 = oit_blend_layer.AddVertexBufferInput(persistent_data.vertex_buf1);
            const FgResRef vtx_buf2 = oit_blend_layer.AddVertexBufferInput(persistent_data.vertex_buf2);
            const FgResRef ndx_buf = oit_blend_layer.AddIndexBufferInput(persistent_data.indices_buf);

            const FgResRef materials_buf =
                oit_blend_layer.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
            const FgResRef textures_buf =
                oit_blend_layer.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
            const FgResRef textures_buf = {};
#endif

            const FgResRef noise_tex =
                oit_blend_layer.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
            const FgResRef white_tex =
                oit_blend_layer.AddTextureInput(dummy_white_, Stg::VertexShader | Stg::FragmentShader);
            const FgResRef instances_buf =
                oit_blend_layer.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
            const FgResRef instances_indices_buf =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

            const FgResRef shader_data_buf = oit_blend_layer.AddUniformBufferInput(
                common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

            const FgResRef cells_buf =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
            const FgResRef items_buf =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
            const FgResRef lights_buf =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
            const FgResRef decals_buf =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

            const FgResRef shadow_map =
                oit_blend_layer.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
            const FgResRef ltc_luts_tex = oit_blend_layer.AddTextureInput(ltc_luts_, Stg::FragmentShader);
            const FgResRef env_tex = oit_blend_layer.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);

            frame_textures.depth = oit_blend_layer.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
            frame_textures.color = oit_blend_layer.AddColorOutput(frame_textures.color);
            oit_depth_buf = oit_blend_layer.AddStorageReadonlyInput(oit_depth_buf, Stg::FragmentShader);

            const int layer_index = oit_layer_count - 1 - i;

            FgResRef specular_tex;
            if (layer_index < OIT_REFLECTION_LAYERS && settings.reflections_quality != eReflectionsQuality::Off) {
                specular_tex = oit_blend_layer.AddTextureInput(oit_specular[layer_index], Stg::FragmentShader);
            }

            FgResRef irradiance_tex, distance_tex, offset_tex;
            if (settings.gi_quality != eGIQuality::Off && frame_textures.gi_cache_irradiance) {
                irradiance_tex =
                    oit_blend_layer.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::FragmentShader);
                distance_tex = oit_blend_layer.AddTextureInput(frame_textures.gi_cache_distance, Stg::FragmentShader);
                offset_tex = oit_blend_layer.AddTextureInput(frame_textures.gi_cache_offset, Stg::FragmentShader);
            }

            back_color = oit_blend_layer.AddTextureInput(back_color, Stg::FragmentShader);
            back_depth = oit_blend_layer.AddTextureInput(back_depth, Stg::FragmentShader);

            oit_blend_layer.make_executor<ExOITBlendLayer>(
                prim_draw_, &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless,
                cells_buf, items_buf, lights_buf, decals_buf, noise_tex, white_tex, shadow_map, ltc_luts_tex, env_tex,
                instances_buf, instances_indices_buf, shader_data_buf, frame_textures.depth, frame_textures.color,
                oit_depth_buf, specular_tex, layer_index, irradiance_tex, distance_tex, offset_tex, back_color,
                back_depth);
        }
    }

    frame_textures.oit_depth_buf = oit_depth_buf;
}