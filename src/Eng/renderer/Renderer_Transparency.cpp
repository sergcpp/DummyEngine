#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"
#include "executors/ExOITBlendLayer.h"
#include "executors/ExOITDepthPeel.h"
#include "executors/ExOITScheduleRays.h"
#include "executors/ExRTSpecular.h"

#include "shaders/rt_specular_interface.h"
#include "shaders/rt_specular_trace_ss_interface.h"
#include "shaders/rt_specular_write_indirect_args_interface.h"

void Eng::Renderer::AddOITPasses(const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                                 const BindlessTextureData &bindless, const FgImgROHandle depth_hierarchy,
                                 const FgBufROHandle rt_geo_instances_res, const FgBufROHandle rt_obj_instances_res,
                                 FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgImgRWHandle oit_specular[OIT_REFLECTION_LAYERS];
    FgBufRWHandle oit_depth, oit_ray_bitmask, oit_ray_counter;

    const int oit_layer_count =
        (settings.transparency_quality == eTransparencyQuality::Ultra) ? OIT_LAYERS_ULTRA : OIT_LAYERS_HIGH;

    { // OIT clear
        auto &oit_clear = fg_builder_.AddNode("OIT CLEAR");

        struct PassData {
            FgBufRWHandle oit_depth;
            FgImgRWHandle oit_specular[OIT_REFLECTION_LAYERS];
            FgBufRWHandle oit_ray_counter;
            FgBufRWHandle oit_ray_bitmask;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        { // output buffer
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Texture;
            desc.size = oit_layer_count * view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);
            desc.views.push_back(Ren::eFormat::R32UI);
            oit_depth = data->oit_depth = oit_clear.AddTransferOutput("Depth Values", desc);
        }

        // specular buffers (halfres)
        for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
            const std::string tex_name = "OIT REFL #" + std::to_string(i);

            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            oit_specular[i] = data->oit_specular[i] = oit_clear.AddClearImageOutput(tex_name, desc);
        }

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 16 * sizeof(uint32_t);

            oit_ray_counter = data->oit_ray_counter = oit_clear.AddTransferOutput("OIT Ray Counter", desc);
        }

        { // ray bitmask (halfres)
            const int w = (((view_state_.ren_res[0] + 1) / 2) + 31) / 32;
            const int h = ((view_state_.ren_res[1] + 1) / 2);

            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = OIT_REFLECTION_LAYERS * (w * h) * sizeof(uint32_t);

            oit_ray_bitmask = data->oit_ray_bitmask = oit_clear.AddTransferOutput("OIT Ray Bitmask", desc);
        }

        oit_clear.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::BufferHandle out_depth = fg.AccessRWBuffer(data->oit_depth);
            const Ren::BufferHandle out_ray_counter = fg.AccessRWBuffer(data->oit_ray_counter);
            const Ren::BufferHandle out_ray_bitmask = fg.AccessRWBuffer(data->oit_ray_bitmask);

            { // OIT Ray Counter
                const auto &[buf_main, buf_cold] = fg.storages().buffers[out_ray_counter];
                Buffer_Fill(fg.ren_ctx().api(), buf_main, 0, buf_cold.size, 0, fg.cmd_buf());
            }

            if (p_list_->alpha_blend_start_index != -1) {
                const auto &[out_depth_main, out_depth_cold] = fg.storages().buffers[out_depth];
                Buffer_Fill(fg.ren_ctx().api(), out_depth_main, 0, out_depth_cold.size, 0, fg.cmd_buf());

                const auto &[out_ray_bitmask_main, out_ray_bitmask_cold] = fg.storages().buffers[out_ray_bitmask];
                Buffer_Fill(fg.ren_ctx().api(), out_ray_bitmask_main, 0, out_ray_bitmask_cold.size, 0, fg.cmd_buf());
            }
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                const Ren::ImageRWHandle oit_specular = fg.AccessRWImage(data->oit_specular[i]);
                if (p_list_->alpha_blend_start_index != -1) {
                    fg.ren_ctx().CmdClearImage(oit_specular, {}, fg.cmd_buf());
                }
            }
        });
    }

    { // depth peel pass
        auto &oit_depth_peel = fg_builder_.AddNode("OIT DEPTH PEEL");
        const FgBufROHandle vtx_buf1 = oit_depth_peel.AddVertexBufferInput(common_buffers.vertex_buf1);
        const FgBufROHandle vtx_buf2 = oit_depth_peel.AddVertexBufferInput(common_buffers.vertex_buf2);
        const FgBufROHandle ndx_buf = oit_depth_peel.AddIndexBufferInput(common_buffers.indices_buf);

        const FgBufROHandle materials =
            oit_depth_peel.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);

        [[maybe_unused]] const FgImgROHandle noise =
            oit_depth_peel.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        const FgImgROHandle white = oit_depth_peel.AddTextureInput(
            frame_textures.dummy_white, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        const FgBufROHandle instances =
            oit_depth_peel.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
        const FgBufROHandle instances_indices =
            oit_depth_peel.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

        const FgBufROHandle shader_data = oit_depth_peel.AddUniformBufferInput(
            common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

        frame_textures.depth = oit_depth_peel.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);
        oit_depth = oit_depth_peel.AddStorageOutput(oit_depth, Stg::FragmentShader);

        oit_depth_peel.make_executor<ExOITDepthPeel>(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials,
                                                     &bindless, white, instances, instances_indices, shader_data,
                                                     frame_textures.depth, oit_depth);
    }

    FgBufRWHandle oit_ray_list;

    { // schedule reflection rays
        auto &oit_schedule = fg_builder_.AddNode("OIT SCHEDULE");

        const FgBufROHandle vtx_buf1 = oit_schedule.AddVertexBufferInput(common_buffers.vertex_buf1);
        const FgBufROHandle vtx_buf2 = oit_schedule.AddVertexBufferInput(common_buffers.vertex_buf2);
        const FgBufROHandle ndx_buf = oit_schedule.AddIndexBufferInput(common_buffers.indices_buf);

        const FgBufROHandle materials =
            oit_schedule.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);

        [[maybe_unused]] const FgImgROHandle noise =
            oit_schedule.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        const FgImgROHandle white = oit_schedule.AddTextureInput(frame_textures.dummy_white,
                                                                 Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        const FgBufROHandle instances =
            oit_schedule.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
        const FgBufROHandle instances_indices =
            oit_schedule.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

        const FgBufROHandle shader_data = oit_schedule.AddUniformBufferInput(
            common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

        frame_textures.depth = oit_schedule.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);
        oit_schedule.AddStorageReadonlyInput(oit_depth, Stg::FragmentShader);

        oit_ray_counter = oit_schedule.AddStorageOutput(oit_ray_counter, Stg::FragmentShader);
        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = OIT_REFLECTION_LAYERS * ((view_state_.ren_res[0] + 1) / 2) *
                        ((view_state_.ren_res[1] + 1) / 2) * 2 * sizeof(uint32_t);

            oit_ray_list = oit_schedule.AddStorageOutput("OIT Ray List", desc, Stg::FragmentShader);
        }
        oit_ray_bitmask = oit_schedule.AddStorageOutput(oit_ray_bitmask, Stg::FragmentShader);

        oit_schedule.make_executor<ExOITScheduleRays>(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials,
                                                      &bindless, noise, white, instances, instances_indices,
                                                      shader_data, frame_textures.depth, oit_depth, oit_ray_counter,
                                                      oit_ray_list, oit_ray_bitmask);
    }

    FgBufROHandle indir_disp;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("OIT INDIR ARGS");

        struct PassData {
            FgBufRWHandle ray_counter;
            FgBufRWHandle indir_disp;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        oit_ray_counter = data->ray_counter = write_indir.AddStorageOutput(oit_ray_counter, Stg::ComputeShader);

        { // Indirect arguments
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 3 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp = data->indir_disp =
                write_indir.AddStorageOutput("OIT Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](const FgContext &fg) {
            using namespace RTSpecularWriteIndirectArgs;

            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle indir_args = fg.AccessRWBuffer(data->indir_disp);

            const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                             {Trg::SBufRW, INDIR_ARGS_SLOT, indir_args}};

            DispatchCompute(fg.cmd_buf(), pi_specular_write_indirect_[0], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                            bindings, nullptr, 0, fg.descr_alloc(), fg.log());
        });
    }

    FgBufRWHandle ray_rt_list;

    if (settings.reflections_quality != eReflectionsQuality::Raytraced_High) {
        auto &spec_trace_ss = fg_builder_.AddNode("OIT SPEC SS");

        struct PassData {
            FgBufROHandle oit_depth;
            FgBufROHandle shared_data;
            FgImgROHandle color, normal;
            FgImgROHandle depth_hierarchy;

            FgImgROHandle albedo, specular;
            FgImgROHandle irradiance, distance, offset;
            FgImgROHandle ltc_luts;

            FgBufROHandle in_ray_list, indir_args;
            FgBufRWHandle inout_ray_counter;
            FgImgRWHandle out_ssr[OIT_REFLECTION_LAYERS];
            FgBufRWHandle out_ray_list;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->oit_depth = spec_trace_ss.AddStorageReadonlyInput(oit_depth, Stg::ComputeShader);
        data->shared_data = spec_trace_ss.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->color = spec_trace_ss.AddTextureInput(frame_textures.color, Stg::ComputeShader);
        data->normal = spec_trace_ss.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->depth_hierarchy = spec_trace_ss.AddTextureInput(depth_hierarchy, Stg::ComputeShader);

        if (frame_textures.gi_cache_irradiance) {
            data->albedo = spec_trace_ss.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
            data->specular = spec_trace_ss.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->ltc_luts = spec_trace_ss.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);
            data->irradiance = spec_trace_ss.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance = spec_trace_ss.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset = spec_trace_ss.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);
        }

        data->in_ray_list = spec_trace_ss.AddStorageReadonlyInput(oit_ray_list, Stg::ComputeShader);
        data->indir_args = spec_trace_ss.AddIndirectBufferInput(indir_disp);
        data->inout_ray_counter = oit_ray_counter = spec_trace_ss.AddStorageOutput(oit_ray_counter, Stg::ComputeShader);
        for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
            oit_specular[i] = data->out_ssr[i] =
                spec_trace_ss.AddStorageImageOutput(oit_specular[i], Stg::ComputeShader);
        }

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = OIT_REFLECTION_LAYERS * ((view_state_.ren_res[0] + 1) / 2) *
                        ((view_state_.ren_res[1] + 1) / 2) * 2 * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list =
                spec_trace_ss.AddStorageOutput("OIT RT Ray List", desc, Stg::ComputeShader);
        }

        spec_trace_ss.set_execute_cb([this, data](const FgContext &fg) {
            using namespace RTSpecularTraceSS;

            const Ren::BufferROHandle oit_depth = fg.AccessROBuffer(data->oit_depth);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle color = fg.AccessROImage(data->color);
            const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle depth_hierarchy = fg.AccessROImage(data->depth_hierarchy);
            const Ren::BufferROHandle in_ray_list = fg.AccessROBuffer(data->in_ray_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            Ren::ImageROHandle albedo, specular;
            Ren::ImageROHandle irr, dist, off;
            Ren::ImageROHandle ltc_luts;
            if (data->irradiance) {
                albedo = fg.AccessROImage(data->albedo);
                specular = fg.AccessROImage(data->specular);
                ltc_luts = fg.AccessROImage(data->ltc_luts);

                irr = fg.AccessROImage(data->irradiance);
                dist = fg.AccessROImage(data->distance);
                off = fg.AccessROImage(data->offset);
            }

            Ren::ImageRWHandle out_refl[OIT_REFLECTION_LAYERS] = {};
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                out_refl[i] = fg.AccessRWImage(data->out_ssr[i]);
            }
            const Ren::BufferHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);
            const Ren::BufferHandle out_ray_list = fg.AccessRWBuffer(data->out_ray_list);

            Ren::SmallVector<Ren::Binding, 24> bindings = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                           {Trg::TexSampled, DEPTH_TEX_SLOT, depth_hierarchy},
                                                           {Trg::TexSampled, COLOR_TEX_SLOT, color},
                                                           {Trg::TexSampled, NORM_TEX_SLOT, normal},
                                                           {Trg::UTBuf, OIT_DEPTH_BUF_SLOT, oit_depth},
                                                           {Trg::SBufRO, IN_RAY_LIST_SLOT, in_ray_list},
                                                           {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, inout_ray_counter},
                                                           {Trg::SBufRW, OUT_RAY_LIST_SLOT, out_ray_list}};
            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                bindings.emplace_back(Trg::ImageRW, OUT_REFL_IMG_SLOT, i, 1, out_refl[i]);
            }
            if (irr) {
                bindings.emplace_back(Trg::TexSampled, ALBEDO_TEX_SLOT, albedo);
                bindings.emplace_back(Trg::TexSampled, SPEC_TEX_SLOT, specular);
                bindings.emplace_back(Trg::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts);

                bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr);
                bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, dist);
                bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, off);
            }

            Params uniform_params;
            uniform_params.resolution = Ren::Vec4u(view_state_.ren_res[0], view_state_.ren_res[1], 0, 0);

            DispatchComputeIndirect(fg.cmd_buf(), pi_specular_trace_ss_[1][bool(irr)], fg.storages(), indir_args, 0,
                                    bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_structs.rt_tlas_buf[int(eTLASIndex::Main)] &&
        int(settings.reflections_quality) >= int(eReflectionsQuality::Raytraced_Normal)) {
        FgBufRWHandle indir_rt_disp;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("OIT RT ARGS");

            struct PassData {
                FgBufRWHandle ray_counter;
                FgBufRWHandle indir_disp;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            oit_ray_counter = data->ray_counter = rt_disp_args.AddStorageOutput(oit_ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp = data->indir_disp =
                    rt_disp_args.AddStorageOutput("OIT RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTSpecularWriteIndirectArgs;

                const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                Params params = {};
                params.counter_index = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 1 : 6;

                DispatchCompute(fg.cmd_buf(), pi_specular_write_indirect_[1], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                bindings, &params, sizeof(params), fg.descr_alloc(), fg.log());
            });
        }

        FgBufRWHandle ray_hits;

        { // Trace reflection rays
            auto &rt_spec = fg_builder_.AddNode("OIT RT SPEC");

            const auto stage = Stg::ComputeShader;

            auto *data = fg_builder_.AllocTempData<ExRTSpecular::Args>();
            data->layered = true;

            data->geo_data = rt_spec.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_spec.AddStorageReadonlyInput(common_buffers.materials, stage);
            data->vtx_buf1 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
            data->vtx_buf2 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf2, stage);
            data->ndx_buf = rt_spec.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
            data->shared_data = rt_spec.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->depth = rt_spec.AddTextureInput(frame_textures.depth, stage);
            data->normal = rt_spec.AddTextureInput(frame_textures.normal, stage);
            if (ray_rt_list) {
                data->ray_list = rt_spec.AddStorageReadonlyInput(ray_rt_list, stage);
            } else {
                data->ray_list = rt_spec.AddStorageReadonlyInput(oit_ray_list, stage);
            }
            data->indir_args = rt_spec.AddIndirectBufferInput(indir_rt_disp);
            data->tlas_buf = rt_spec.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = acc_structs.swrt.rt_root_node;
                data->swrt.rt_blas = rt_spec.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx = rt_spec.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
                data->swrt.mesh_instances = rt_spec.AddStorageReadonlyInput(rt_obj_instances_res, stage);
            }

            data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

            data->oit_depth = rt_spec.AddStorageReadonlyInput(oit_depth, stage);

            oit_ray_counter = data->inout_ray_counter = rt_spec.AddStorageOutput(oit_ray_counter, stage);

            { // Ray hit results
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size = OIT_REFLECTION_LAYERS * RTSpecular::RAY_HITS_STRIDE * ((view_state_.ren_res[0] + 1) / 2) *
                            ((view_state_.ren_res[1] + 1) / 2) * sizeof(uint32_t);

                ray_hits = data->out_ray_hits =
                    rt_spec.AddStorageOutput("OIT RT Hits Buf", desc, Ren::eStage::ComputeShader);
            }

            rt_spec.make_executor<ExRTSpecular>(&view_state_, &bindless, data);
        }

        { // Prepare arguments for shading dispatch
            auto &rt_shade_args = fg_builder_.AddNode("OIT RT SHADE ARGS");

            struct PassData {
                FgBufRWHandle ray_counter;
                FgBufRWHandle indir_disp;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            oit_ray_counter = data->ray_counter = rt_shade_args.AddStorageOutput(oit_ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp = data->indir_disp =
                    rt_shade_args.AddStorageOutput("OIT RT Shade Args", desc, Stg::ComputeShader);
            }

            rt_shade_args.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTSpecularWriteIndirectArgs;

                const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                DispatchCompute(fg.cmd_buf(), pi_specular_write_indirect_[2], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                bindings, nullptr, 0, fg.descr_alloc(), fg.log());
            });
        }

        { // Shade ray hits
            auto &spec_shade = fg_builder_.AddNode("OIT RT SHADE");

            struct PassData {
                FgBufROHandle oit_depth;
                FgBufROHandle shared_data;
                FgImgROHandle depth, normal;
                FgImgROHandle env;
                FgBufROHandle mesh_instances;
                FgBufROHandle geo_data;
                FgBufROHandle materials;
                FgBufROHandle vtx_data0_buf, vtx_data1_buf;
                FgBufROHandle ndx_buf;
                FgBufROHandle lights;
                FgImgROHandle shadow_depth, shadow_color;
                FgImgROHandle ltc_luts;
                FgBufROHandle cells;
                FgBufROHandle items;

                FgImgROHandle irradiance;
                FgImgROHandle distance;
                FgImgROHandle offset;

                FgBufROHandle in_ray_list, in_ray_hits, indir_args;
                FgBufRWHandle inout_ray_counter;
                FgImgRWHandle out_specular[OIT_REFLECTION_LAYERS];
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->oit_depth = spec_shade.AddStorageReadonlyInput(oit_depth, Stg::ComputeShader);
            data->shared_data = spec_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth = spec_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->normal = spec_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->env = spec_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
            data->mesh_instances = spec_shade.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
            data->geo_data = spec_shade.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
            data->materials = spec_shade.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
            data->vtx_data0_buf = spec_shade.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
            data->vtx_data1_buf = spec_shade.AddStorageReadonlyInput(common_buffers.vertex_buf2, Stg::ComputeShader);
            data->ndx_buf = spec_shade.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
            data->lights = spec_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
            data->shadow_depth = spec_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
            data->shadow_color = spec_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
            data->ltc_luts = spec_shade.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);
            data->cells = spec_shade.AddStorageReadonlyInput(common_buffers.rt_cells, Stg::ComputeShader);
            data->items = spec_shade.AddStorageReadonlyInput(common_buffers.rt_items, Stg::ComputeShader);

            data->irradiance = spec_shade.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance = spec_shade.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset = spec_shade.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

            if (ray_rt_list) {
                data->in_ray_list = spec_shade.AddStorageReadonlyInput(ray_rt_list, Stg::ComputeShader);
            } else {
                data->in_ray_list = spec_shade.AddStorageReadonlyInput(oit_ray_list, Stg::ComputeShader);
            }
            data->in_ray_hits = spec_shade.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
            data->indir_args = spec_shade.AddIndirectBufferInput(indir_rt_disp);
            oit_ray_counter = data->inout_ray_counter =
                spec_shade.AddStorageOutput(oit_ray_counter, Stg::ComputeShader);

            for (int i = 0; i < OIT_REFLECTION_LAYERS; ++i) {
                oit_specular[i] = data->out_specular[i] =
                    spec_shade.AddStorageImageOutput(oit_specular[i], Stg::ComputeShader);
            }

            spec_shade.set_execute_cb([this, &bindless, data](const FgContext &fg) {
                using namespace RTSpecular;

                const Ren::BufferROHandle oit_depth = fg.AccessROBuffer(data->oit_depth);
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
                const Ren::ImageROHandle env = fg.AccessROImage(data->env);
                const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(data->mesh_instances);
                const Ren::BufferROHandle geo_data = fg.AccessROBuffer(data->geo_data);
                const Ren::BufferROHandle materials = fg.AccessROBuffer(data->materials);
                const Ren::BufferROHandle vtx_data0_buf = fg.AccessROBuffer(data->vtx_data0_buf);
                const Ren::BufferROHandle vtx_data1_buf = fg.AccessROBuffer(data->vtx_data1_buf);
                const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(data->ndx_buf);
                const Ren::BufferROHandle lights = fg.AccessROBuffer(data->lights);
                const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
                const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);
                const Ren::ImageROHandle ltc_luts = fg.AccessROImage(data->ltc_luts);
                const Ren::BufferROHandle cells = fg.AccessROBuffer(data->cells);
                const Ren::BufferROHandle items = fg.AccessROBuffer(data->items);

                const Ren::ImageROHandle irr = fg.AccessROImage(data->irradiance);
                const Ren::ImageROHandle dist = fg.AccessROImage(data->distance);
                const Ren::ImageROHandle off = fg.AccessROImage(data->offset);

                const Ren::BufferROHandle in_ray_list = fg.AccessROBuffer(data->in_ray_list);
                const Ren::BufferROHandle in_ray_hits = fg.AccessROBuffer(data->in_ray_hits);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);
                const Ren::BufferHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);

                Ren::SmallVector<Ren::Binding, 16> bindings = {
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                    {Trg::BindlessDescriptors, BIND_BINDLESS_TEX, bindless.rt_inline_textures},
                    {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                    {Trg::TexSampled, NORM_TEX_SLOT, normal},
                    {Trg::UTBuf, OIT_DEPTH_BUF_SLOT, oit_depth},
                    {Trg::TexSampled, ENV_TEX_SLOT, env},
                    {Trg::UTBuf, MESH_INSTANCES_BUF_SLOT, mesh_instances},
                    {Trg::SBufRO, GEO_DATA_BUF_SLOT, geo_data},
                    {Trg::SBufRO, MATERIAL_BUF_SLOT, materials},
                    {Trg::UTBuf, VTX_BUF1_SLOT, vtx_data0_buf},
                    {Trg::UTBuf, VTX_BUF2_SLOT, vtx_data1_buf},
                    {Trg::UTBuf, NDX_BUF_SLOT, ndx_buf},
                    {Trg::SBufRO, LIGHTS_BUF_SLOT, lights},
                    {Trg::TexSampled, SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                    {Trg::TexSampled, SHADOW_COLOR_TEX_SLOT, shadow_color},
                    {Trg::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts},
                    {Trg::UTBuf, CELLS_BUF_SLOT, cells},
                    {Trg::UTBuf, ITEMS_BUF_SLOT, items},
                    {Trg::SBufRO, RAY_LIST_SLOT, in_ray_list},
                    {Trg::SBufRO, RAY_HITS_BUF_SLOT, in_ray_hits},
                    {Trg::SBufRW, RAY_COUNTER_SLOT, inout_ray_counter}};

                for (int i = 0; i < OIT_REFLECTION_LAYERS && data->out_specular[i]; ++i) {
                    const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->out_specular[i]);
                    bindings.emplace_back(Ren::eBindTarget::ImageRW, RTSpecular::OUT_REFL_IMG_SLOT, i, 1, out_refl);
                }

                RTSpecular::Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.pixel_spread_angle = 2.0f * view_state_.pixel_spread_angle;
                uniform_params.is_hwrt = ctx_.capabilities.hwrt ? 1 : 0;

                // Shade misses
                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_shade_[7], fg.storages(), indir_args, 0, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());

                bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr);
                bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, dist);
                bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, off);

                // Shade hits
                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_shade_[8], fg.storages(), indir_args,
                                        sizeof(Ren::DispatchIndirectCommand), bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });
        }
    }

    for (int i = 0; i < oit_layer_count; ++i) {
        FgImgRWHandle back_color, back_depth;
        { // copy background
            const std::string pass_name = "OIT BACK #" + std::to_string(i);

            auto &oit_back = fg_builder_.AddNode(pass_name);

            struct PassData {
                FgImgROHandle in_back_color;
                FgImgROHandle in_back_depth;

                FgImgRWHandle out_back_color;
                FgImgRWHandle out_back_depth;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->in_back_color = oit_back.AddTransferImageInput(frame_textures.color);
            data->in_back_depth = oit_back.AddTransferImageInput(frame_textures.depth);
            { // background color
                const std::string tex_name = "OIT Back Color #" + std::to_string(i);
                back_color = data->out_back_color =
                    oit_back.AddTransferImageOutput(tex_name, frame_textures.color_desc);
            }
            { // background depth
                const std::string tex_name = i == 0 ? OPAQUE_DEPTH_TEX : "OIT Back Depth #" + std::to_string(i);
                back_depth = data->out_back_depth =
                    oit_back.AddTransferImageOutput(tex_name, frame_textures.depth_desc);
                if (i == 0) {
                    frame_textures.opaque_depth = back_depth;
                }
            }

            oit_back.set_execute_cb([this, data, i](const FgContext &fg) {
                const Ren::ImageROHandle in_back_color = fg.AccessROImage(data->in_back_color);
                const Ren::ImageROHandle in_back_depth = fg.AccessROImage(data->in_back_depth);

                const Ren::ImageRWHandle out_back_color = fg.AccessRWImage(data->out_back_color);
                const Ren::ImageRWHandle out_back_depth = fg.AccessRWImage(data->out_back_depth);

                // TODO: Get rid of this!
                const auto &[img_main, img_cold] = fg.storages().images[in_back_color];
                const int w = img_cold.params.w, h = img_cold.params.h;

                if (p_list_->alpha_blend_start_index != -1) {
                    fg.ren_ctx().CmdCopyImageToImage(fg.cmd_buf(), in_back_color, 0, Ren::Vec3i{0, 0, 0},
                                                     out_back_color, 0, Ren::Vec3i{0, 0, 0}, 0, Ren::Vec3i{w, h, 1});
                }
                if (i == 0 || p_list_->alpha_blend_start_index != -1) {
                    fg.ren_ctx().CmdCopyImageToImage(fg.cmd_buf(), in_back_depth, 0, Ren::Vec3i{0, 0, 0},
                                                     out_back_depth, 0, Ren::Vec3i{0, 0, 0}, 0, Ren::Vec3i{w, h, 1});
                }
            });
        }
        { // blend
            const std::string pass_name = "OIT BLEND #" + std::to_string(i);

            auto &oit_blend_layer = fg_builder_.AddNode(pass_name);
            const FgBufROHandle vtx_buf1 = oit_blend_layer.AddVertexBufferInput(common_buffers.vertex_buf1);
            const FgBufROHandle vtx_buf2 = oit_blend_layer.AddVertexBufferInput(common_buffers.vertex_buf2);
            const FgBufROHandle ndx_buf = oit_blend_layer.AddIndexBufferInput(common_buffers.indices_buf);

            const FgBufROHandle materials =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);

            const FgImgROHandle noise = oit_blend_layer.AddTextureInput(
                frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
            const FgImgROHandle white = oit_blend_layer.AddTextureInput(
                frame_textures.dummy_white, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
            const FgBufROHandle instances =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
            const FgBufROHandle instances_indices =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

            const FgBufROHandle shader_data = oit_blend_layer.AddUniformBufferInput(
                common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

            const FgBufROHandle cells =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
            const FgBufROHandle items =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
            const FgBufROHandle lights =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
            const FgBufROHandle decals =
                oit_blend_layer.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

            const FgImgROHandle shadow_map =
                oit_blend_layer.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
            const FgImgROHandle ltc_luts =
                oit_blend_layer.AddTextureInput(frame_textures.ltc_luts, Stg::FragmentShader);
            const FgImgROHandle envmap = oit_blend_layer.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);

            frame_textures.depth = oit_blend_layer.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);
            frame_textures.color = oit_blend_layer.AddColorOutput(frame_textures.color);
            const FgBufROHandle oit_depth_ro = oit_blend_layer.AddStorageReadonlyInput(oit_depth, Stg::FragmentShader);

            const int layer_index = oit_layer_count - 1 - i;

            FgImgROHandle specular;
            if (layer_index < OIT_REFLECTION_LAYERS && settings.reflections_quality != eReflectionsQuality::Off) {
                specular = oit_blend_layer.AddTextureInput(oit_specular[layer_index], Stg::FragmentShader);
            }

            FgImgROHandle irradiance, distance, offset;
            if (settings.gi_quality != eGIQuality::Off && frame_textures.gi_cache_irradiance) {
                irradiance = oit_blend_layer.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::FragmentShader);
                distance = oit_blend_layer.AddTextureInput(frame_textures.gi_cache_distance, Stg::FragmentShader);
                offset = oit_blend_layer.AddTextureInput(frame_textures.gi_cache_offset, Stg::FragmentShader);
            }

            oit_blend_layer.AddTextureInput(back_color, Stg::FragmentShader);
            oit_blend_layer.AddTextureInput(back_depth, Stg::FragmentShader);

            oit_blend_layer.make_executor<ExOITBlendLayer>(
                prim_draw_, &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials, &bindless, cells, items,
                lights, decals, noise, white, shadow_map, ltc_luts, envmap, instances, instances_indices, shader_data,
                frame_textures.depth, frame_textures.color, oit_depth_ro, specular, layer_index, irradiance, distance,
                offset, back_color, back_depth);
        }
    }

    frame_textures.oit_depth = oit_depth;
}