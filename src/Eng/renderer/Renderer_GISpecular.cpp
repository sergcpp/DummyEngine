#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/blit_ssr_compose_interface.h"
#include "shaders/blit_ssr_dilate_interface.h"
#include "shaders/blit_ssr_interface.h"
#include "shaders/rt_specular_classify_interface.h"
#include "shaders/rt_specular_filter_interface.h"
#include "shaders/rt_specular_interface.h"
#include "shaders/rt_specular_reproject_interface.h"
#include "shaders/rt_specular_stabilization_interface.h"
#include "shaders/rt_specular_temporal_interface.h"
#include "shaders/rt_specular_trace_ss_interface.h"
#include "shaders/rt_specular_write_indirect_args_interface.h"
#include "shaders/tile_clear_interface.h"

#include "executors/ExRTSpecular.h"

#include "Renderer_Names.h"

void Eng::Renderer::AddHQSpecularPasses(const bool deferred_shading, const bool debug_denoise,
                                        const CommonBuffers &common_buffers, const AccelerationStructures &acc_structs,
                                        const BindlessTextureData &bindless, const FgImgROHandle depth_hierarchy,
                                        const FgBufROHandle rt_geo_instances_res,
                                        const FgBufROHandle rt_obj_instances_res, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    // Reflection settings
    const int SamplesPerQuad = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 4 : 1;
    static const bool VarianceGuided = true;
    static const bool EnableBlur = true;
    const bool EnableStabilization = false; //(settings.taa_mode != eTAAMode::Static);

    const bool two_bounces = (settings.reflections_quality == eReflectionsQuality::Raytraced_High);

    FgBufRWHandle ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &ssr_prepare = fg_builder_.AddNode("SPEC PREPARE");

        struct PassData {
            FgBufRWHandle ray_counter;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 16 * sizeof(uint32_t);

            ray_counter = data->ray_counter = ssr_prepare.AddTransferOutput("Ray Counter", desc);
        }

        ssr_prepare.set_execute_cb([data](const FgContext &fg) {
            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);

            const auto &[buf_main, buf_cold] = fg.storages().buffers[ray_counter];
            Buffer_Fill(fg.ren_ctx().api(), buf_main, 0, buf_cold.size, 0, fg.cmd_buf());
        });
    }

    const int tile_count = ((view_state_.ren_res[0] + 7) / 8) * ((view_state_.ren_res[1] + 7) / 8);

    FgBufRWHandle ray_list, tile_list;
    FgImgRWHandle refl, noise;

    { // Classify pixel quads
        auto &spec_classify = fg_builder_.AddNode("SPEC CLASSIFY");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle specular;
            FgImgROHandle normal;
            FgImgROHandle variance_history;
            FgBufROHandle bn_pmj_seq;
            FgBufRWHandle ray_counter;
            FgBufRWHandle ray_list;
            FgBufRWHandle tile_list;
            FgImgRWHandle out_refl, out_noise;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = spec_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->specular = spec_classify.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->normal = spec_classify.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->variance_history = spec_classify.AddHistoryTextureInput(SPECULAR_VARIANCE_TEX, Stg::ComputeShader);
        data->bn_pmj_seq =
            spec_classify.AddStorageReadonlyInput(common_buffers.bn_pmj_2D_64spp_seq, Stg::ComputeShader);
        ray_counter = data->ray_counter = spec_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = spec_classify.AddStorageOutput("Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = tile_count * sizeof(uint32_t);

            tile_list = data->tile_list = spec_classify.AddStorageOutput("Tile List", desc, Stg::ComputeShader);
        }
        { // reflections texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            refl = data->out_refl = spec_classify.AddStorageImageOutput("Spec Temp 2", desc, Stg::ComputeShader);
        }
        { // blue noise texture
            FgImgDesc desc;
            desc.w = desc.h = 128;
            desc.format = Ren::eFormat::RGBA8;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::Repeat;
            noise = data->out_noise = spec_classify.AddStorageImageOutput("BN Tex", desc, Stg::ComputeShader);
        }

        spec_classify.set_execute_cb([this, data, tile_count, SamplesPerQuad](const FgContext &fg) {
            using namespace RTSpecularClassifyTiles;

            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle spec = fg.AccessROImage(data->specular);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle variance = fg.AccessROImage(data->variance_history);
            const Ren::BufferROHandle bn_pmj_seq = fg.AccessROBuffer(data->bn_pmj_seq);

            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle ray_list = fg.AccessRWBuffer(data->ray_list);
            const Ren::BufferHandle tile_list = fg.AccessRWBuffer(data->tile_list);

            const Ren::ImageRWHandle refl = fg.AccessRWImage(data->out_refl);
            const Ren::ImageRWHandle noise = fg.AccessRWImage(data->out_noise);

            const Ren::Binding bindings[] = {
                {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}}, {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                {Trg::TexSampled, NORM_TEX_SLOT, norm},        {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                {Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},  {Trg::SBufRW, RAY_LIST_SLOT, ray_list},
                {Trg::SBufRW, TILE_LIST_SLOT, tile_list},      {Trg::UTBuf, BN_PMJ_SEQ_BUF_SLOT, bn_pmj_seq},
                {Trg::ImageRW, OUT_REFL_IMG_SLOT, refl},       {Trg::ImageRW, OUT_NOISE_IMG_SLOT, noise}};

            const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                    (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.samples_and_guided = Ren::Vec2u(SamplesPerQuad, VarianceGuided ? 1u : 0u);
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.clear = (view_state_.stochastic_lights_count > 0) ? 0.0f : 1.0f;
            uniform_params.tile_count = tile_count;

            DispatchCompute(fg.cmd_buf(), pi_specular_classify_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgBufRWHandle indir_disp;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("SPEC INDIR ARGS");

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

            indir_disp = data->indir_disp = write_indir.AddStorageOutput("Intersect Args", desc, Stg::ComputeShader);
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
        auto &spec_trace_ss = fg_builder_.AddNode("SPEC TRACE SS");

        struct PassData {
            FgImgROHandle noise;
            FgBufROHandle shared_data;
            FgImgROHandle color, normal;
            FgImgROHandle depth_hierarchy;

            FgImgROHandle albedo, specular;
            FgImgROHandle irradiance, distance, offset;
            FgImgROHandle ltc_luts;

            FgBufROHandle in_ray_list, indir_args;
            FgBufRWHandle inout_ray_counter;
            FgImgRWHandle refl;
            FgBufRWHandle out_ray_list;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->noise = spec_trace_ss.AddTextureInput(noise, Stg::ComputeShader);
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

        data->in_ray_list = spec_trace_ss.AddStorageReadonlyInput(ray_list, Stg::ComputeShader);
        data->indir_args = spec_trace_ss.AddIndirectBufferInput(indir_disp);
        ray_counter = data->inout_ray_counter = spec_trace_ss.AddStorageOutput(ray_counter, Stg::ComputeShader);
        refl = data->refl = spec_trace_ss.AddStorageImageOutput(refl, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list = spec_trace_ss.AddStorageOutput("Ray RT List", desc, Stg::ComputeShader);
        }

        spec_trace_ss.set_execute_cb([this, data](const FgContext &fg) {
            using namespace RTSpecularTraceSS;

            const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
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

            const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->refl);
            const Ren::BufferHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);
            const Ren::BufferHandle out_ray_list = fg.AccessRWBuffer(data->out_ray_list);

            Ren::SmallVector<Ren::Binding, 24> bindings = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                           {Trg::TexSampled, DEPTH_TEX_SLOT, depth_hierarchy},
                                                           {Trg::TexSampled, COLOR_TEX_SLOT, color},
                                                           {Trg::TexSampled, NORM_TEX_SLOT, normal},
                                                           {Trg::TexSampled, NOISE_TEX_SLOT, noise},
                                                           {Trg::SBufRO, IN_RAY_LIST_SLOT, in_ray_list},
                                                           {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_refl},
                                                           {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, inout_ray_counter},
                                                           {Trg::SBufRW, OUT_RAY_LIST_SLOT, out_ray_list}};
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

            DispatchComputeIndirect(fg.cmd_buf(), pi_specular_trace_ss_[0][bool(irr)], fg.storages(), indir_args, 0,
                                    bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_structs.rt_tlas_buf[int(eTLASIndex::Main)] &&
        int(settings.reflections_quality) >= int(eReflectionsQuality::Raytraced_Normal)) {
        FgBufRWHandle indir_rt_disp;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("RT DISPATCH ARGS");

            struct PassData {
                FgBufRWHandle ray_counter;
                FgBufRWHandle indir_disp;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            ray_counter = data->ray_counter = rt_disp_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp = data->indir_disp =
                    rt_disp_args.AddStorageOutput("RT Dispatch Args", desc, Stg::ComputeShader);
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
            auto &rt_spec = fg_builder_.AddNode(two_bounces ? "RT SPEC 1ST" : "RT SPEC");

            auto *data = fg_builder_.AllocTempData<ExRTSpecular::Args>();

            const auto stage = Stg::ComputeShader;

            data->geo_data = rt_spec.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_spec.AddStorageReadonlyInput(common_buffers.materials, stage);
            data->vtx_buf1 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
            data->vtx_buf2 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf2, stage);
            data->ndx_buf = rt_spec.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
            data->shared_data = rt_spec.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->noise = rt_spec.AddTextureInput(noise, stage);
            data->depth = rt_spec.AddTextureInput(frame_textures.depth, stage);
            data->normal = rt_spec.AddTextureInput(frame_textures.normal, stage);
            if (ray_rt_list) {
                data->ray_list = rt_spec.AddStorageReadonlyInput(ray_rt_list, stage);
            } else {
                data->ray_list = rt_spec.AddStorageReadonlyInput(ray_list, stage);
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

#if 1
            ray_counter = data->inout_ray_counter = rt_spec.AddStorageOutput(ray_counter, stage);

            { // Ray hit results
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size =
                    RTSpecular::RAY_HITS_STRIDE * view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

                ray_hits = data->out_ray_hits =
                    rt_spec.AddStorageOutput("RT Hits Buf", desc, Ren::eStage::ComputeShader);
            }

            data->second_bounce = false;
#else
            refl = data->out_refl[0] = rt_spec.AddStorageImageOutput(refl, stage);
            data->env = rt_spec.AddTextureInput(frame_textures.envmap, stage);
            data->ray_counter = rt_spec.AddStorageReadonlyInput(ray_counter, stage);

            data->lights = rt_spec.AddStorageReadonlyInput(common_buffers.lights, stage);
            data->shadow_depth = rt_spec.AddTextureInput(frame_textures.shadow_depth, stage);
            data->shadow_color = rt_spec.AddTextureInput(frame_textures.shadow_color, stage);
            data->ltc_luts = rt_spec.AddTextureInput(frame_textures.ltc_luts, stage);
            data->cells = rt_spec.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
            data->items = rt_spec.AddStorageReadonlyInput(common_buffers.rt_items, stage);

            if (settings.gi_quality != eGIQuality::Off) {
                data->irradiance = rt_spec.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
                data->distance = rt_spec.AddTextureInput(frame_textures.gi_cache_distance, stage);
                data->offset = rt_spec.AddTextureInput(frame_textures.gi_cache_offset, stage);
            }

            if (common_buffers.stoch_lights) {
                data->stoch_lights = rt_spec.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
                data->light_nodes =
                    rt_spec.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
            }

            data->two_bounces = two_bounces;
#endif

            data->layered = false;

            rt_spec.make_executor<ExRTSpecular>(&view_state_, &bindless, data);
        }

        { // Prepare arguments for shading dispatch
            auto &rt_shade_args = fg_builder_.AddNode(two_bounces ? "SPEC RT SHADE ARGS 1ST" : "SPEC RT SHADE ARGS");

            struct PassData {
                FgBufRWHandle ray_counter;
                FgBufRWHandle indir_disp;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            ray_counter = data->ray_counter = rt_shade_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp = data->indir_disp =
                    rt_shade_args.AddStorageOutput("Spec RT Shade Args", desc, Stg::ComputeShader);
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

        FgBufRWHandle secondary_ray_list;

        { // Shade ray hits
            auto &spec_shade = fg_builder_.AddNode(two_bounces ? "SPEC SHADE 1ST" : "SPEC SHADE");

            struct PassData {
                FgImgROHandle noise;
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

                FgBufROHandle stoch_lights;
                FgBufROHandle light_nodes;

                FgBufROHandle in_ray_hits, indir_args;
                FgBufRWHandle inout_ray_counter;
                FgImgRWHandle out_specular;
                FgBufRWHandle out_ray_list;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->noise = spec_shade.AddTextureInput(noise, Stg::ComputeShader);
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

            if (common_buffers.stoch_lights) {
                data->stoch_lights =
                    spec_shade.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
                data->light_nodes =
                    spec_shade.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
            }

            data->in_ray_hits = spec_shade.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
            data->indir_args = spec_shade.AddIndirectBufferInput(indir_rt_disp);
            ray_counter = data->inout_ray_counter = spec_shade.AddStorageOutput(ray_counter, Stg::ComputeShader);
            refl = data->out_specular = spec_shade.AddStorageImageOutput(refl, Stg::ComputeShader);

            if (two_bounces) {
                // packed ray list
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Storage;
                desc.size =
                    view_state_.ren_res[0] * view_state_.ren_res[1] * RTSpecular::RAY_LIST_STRIDE * sizeof(uint32_t);

                secondary_ray_list = data->out_ray_list =
                    spec_shade.AddStorageOutput("Spec Secondary Ray List", desc, Stg::ComputeShader);
            }

            spec_shade.set_execute_cb([this, &bindless, two_bounces, data](const FgContext &fg) {
                using namespace RTSpecular;

                const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
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

                Ren::BufferROHandle stoch_lights = {}, light_nodes = {};
                if (data->stoch_lights) {
                    stoch_lights = fg.AccessROBuffer(data->stoch_lights);
                    light_nodes = fg.AccessROBuffer(data->light_nodes);
                }

                const Ren::BufferROHandle in_ray_hits = fg.AccessROBuffer(data->in_ray_hits);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);
                const Ren::BufferHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);

                const Ren::ImageRWHandle out_specular = fg.AccessRWImage(data->out_specular);

                Ren::BufferHandle out_ray_list = {};
                if (data->out_ray_list) {
                    out_ray_list = fg.AccessRWBuffer(data->out_ray_list);
                }

                Ren::SmallVector<Ren::Binding, 16> bindings = {
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                    {Trg::BindlessDescriptors, BIND_BINDLESS_TEX, bindless.rt_inline_textures},
                    {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                    {Trg::TexSampled, NORM_TEX_SLOT, normal},
                    {Trg::TexSampled, NOISE_TEX_SLOT, noise},
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
                    {Trg::SBufRO, RAY_HITS_BUF_SLOT, in_ray_hits},
                    {Trg::SBufRW, RAY_COUNTER_SLOT, inout_ray_counter},
                    {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_specular}};

                RTSpecular::Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
                // uniform_params.frame_index = view_state_.frame_index;
                uniform_params.lights_count = view_state_.stochastic_lights_count;
                uniform_params.is_hwrt = ctx_.capabilities.hwrt ? 1 : 0;

                // Shade misses
                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_shade_[0], fg.storages(), indir_args, 0, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());

                bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr);
                bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, dist);
                bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, off);
                if (view_state_.stochastic_lights_count != 0 && stoch_lights) {
                    bindings.emplace_back(Trg::UTBuf, STOCH_LIGHTS_BUF_SLOT, stoch_lights);
                    bindings.emplace_back(Trg::UTBuf, LIGHT_NODES_BUF_SLOT, light_nodes);
                }
                if (two_bounces && out_ray_list) {
                    bindings.emplace_back(Trg::SBufRW, OUT_RAY_LIST_BUF_SLOT, out_ray_list);
                }

                // Shade hits
                const Ren::PipelineHandle pi =
                    two_bounces ? pi_specular_shade_[4 + int(view_state_.stochastic_lights_count != 0)]
                                : pi_specular_shade_[2 + int(view_state_.stochastic_lights_count != 0)];
                DispatchComputeIndirect(fg.cmd_buf(), pi, fg.storages(), indir_args,
                                        sizeof(Ren::DispatchIndirectCommand), bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });
        }

        if (two_bounces) {
            { // Prepare arguments for indirect RT dispatch
                auto &rt_disp_args = fg_builder_.AddNode("SPEC RT DISP ARGS 2ND");

                struct PassData {
                    FgBufRWHandle ray_counter;
                    FgBufRWHandle indir_disp;
                };

                auto *data = fg_builder_.AllocTempData<PassData>();
                ray_counter = data->ray_counter = rt_disp_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

                { // Indirect arguments
                    FgBufDesc desc = {};
                    desc.type = Ren::eBufType::Indirect;
                    desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                    indir_rt_disp = data->indir_disp =
                        rt_disp_args.AddStorageOutput("Spec RT Secondary Dispatch Args", desc, Stg::ComputeShader);
                }

                rt_disp_args.set_execute_cb([this, data](const FgContext &fg) {
                    using namespace RTSpecularWriteIndirectArgs;

                    const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                    const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                    const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                     {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                    Params params = {};
                    params.counter_index = 6;

                    DispatchCompute(fg.cmd_buf(), pi_specular_write_indirect_[1], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                    bindings, &params, sizeof(params), fg.descr_alloc(), fg.log());
                });
            }

            FgBufRWHandle secondary_ray_hits;

            { // Trace reflection rays
                auto &rt_spec = fg_builder_.AddNode("RT SPEC 2ND");

                auto *data = fg_builder_.AllocTempData<ExRTSpecular::Args>();
                data->layered = false;
                data->second_bounce = true;

                const auto stage = Stg::ComputeShader;

                data->geo_data = rt_spec.AddStorageReadonlyInput(rt_geo_instances_res, stage);
                data->materials = rt_spec.AddStorageReadonlyInput(common_buffers.materials, stage);
                data->vtx_buf1 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
                data->vtx_buf2 = rt_spec.AddStorageReadonlyInput(common_buffers.vertex_buf2, stage);
                data->ndx_buf = rt_spec.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
                data->shared_data = rt_spec.AddUniformBufferInput(common_buffers.shared_data, stage);
                data->noise = rt_spec.AddTextureInput(noise, stage);
                data->depth = rt_spec.AddTextureInput(frame_textures.depth, stage);
                data->normal = rt_spec.AddTextureInput(frame_textures.normal, stage);
                data->ray_list = rt_spec.AddStorageReadonlyInput(secondary_ray_list, stage);
                data->indir_args = rt_spec.AddIndirectBufferInput(indir_rt_disp);
                data->tlas_buf = rt_spec.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);

                if (!ctx_.capabilities.hwrt) {
                    data->swrt.root_node = acc_structs.swrt.rt_root_node;
                    data->swrt.rt_blas = rt_spec.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
                    data->swrt.prim_ndx = rt_spec.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
                    data->swrt.mesh_instances = rt_spec.AddStorageReadonlyInput(rt_obj_instances_res, stage);
                }

                data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

                ray_counter = data->inout_ray_counter = rt_spec.AddStorageOutput(ray_counter, stage);

                { // Ray hit results
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size = RTSpecular::RAY_HITS_STRIDE * view_state_.ren_res[0] * view_state_.ren_res[1] *
                                sizeof(uint32_t);

                    secondary_ray_hits = data->out_ray_hits =
                        rt_spec.AddStorageOutput("Spec RT Secondary Hits Buf", desc, Ren::eStage::ComputeShader);
                }

                rt_spec.make_executor<ExRTSpecular>(&view_state_, &bindless, data);
            }

            { // Prepare arguments for shading dispatch
                auto &rt_shade_args = fg_builder_.AddNode("SPEC RT SHADE ARGS 2ND");

                struct PassData {
                    FgBufRWHandle ray_counter;
                    FgBufRWHandle indir_disp;
                };

                auto *data = fg_builder_.AllocTempData<PassData>();
                ray_counter = data->ray_counter = rt_shade_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

                { // Indirect arguments
                    FgBufDesc desc = {};
                    desc.type = Ren::eBufType::Indirect;
                    desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

                    indir_rt_disp = data->indir_disp =
                        rt_shade_args.AddStorageOutput("Spec RT Secondary Shade Args", desc, Stg::ComputeShader);
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
                auto &spec_shade = fg_builder_.AddNode("SPEC SHADE 2ND");

                struct PassData {
                    FgImgROHandle noise;
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
                    FgImgRWHandle out_specular;
                };

                auto *data = fg_builder_.AllocTempData<PassData>();
                data->noise = spec_shade.AddTextureInput(noise, Stg::ComputeShader);
                data->shared_data = spec_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
                data->depth = spec_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
                data->normal = spec_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
                data->env = spec_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
                data->mesh_instances = spec_shade.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
                data->geo_data = spec_shade.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
                data->materials = spec_shade.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
                data->vtx_data0_buf =
                    spec_shade.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
                data->vtx_data1_buf =
                    spec_shade.AddStorageReadonlyInput(common_buffers.vertex_buf2, Stg::ComputeShader);
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

                data->in_ray_list = spec_shade.AddStorageReadonlyInput(secondary_ray_list, Stg::ComputeShader);
                data->in_ray_hits = spec_shade.AddStorageReadonlyInput(secondary_ray_hits, Stg::ComputeShader);
                data->indir_args = spec_shade.AddIndirectBufferInput(indir_rt_disp);
                ray_counter = data->inout_ray_counter = spec_shade.AddStorageOutput(ray_counter, Stg::ComputeShader);
                refl = data->out_specular = spec_shade.AddStorageImageOutput(refl, Stg::ComputeShader);

                spec_shade.set_execute_cb([this, &bindless, data](const FgContext &fg) {
                    using namespace RTSpecular;

                    const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
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
                    const Ren::ImageRWHandle out_specular = fg.AccessRWImage(data->out_specular);

                    Ren::SmallVector<Ren::Binding, 16> bindings = {
                        {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                        {Trg::BindlessDescriptors, BIND_BINDLESS_TEX, bindless.rt_inline_textures},
                        {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                        {Trg::TexSampled, NORM_TEX_SLOT, normal},
                        {Trg::TexSampled, NOISE_TEX_SLOT, noise},
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
                        {Trg::SBufRW, RAY_COUNTER_SLOT, inout_ray_counter},
                        {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_specular}};

                    RTSpecular::Params uniform_params;
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
                    // uniform_params.frame_index = view_state_.frame_index;
                    uniform_params.lights_count = view_state_.stochastic_lights_count;
                    uniform_params.is_hwrt = ctx_.capabilities.hwrt ? 1 : 0;

                    // Shade misses
                    DispatchComputeIndirect(fg.cmd_buf(), pi_specular_shade_[1], fg.storages(), indir_args, 0, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());

                    bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr);
                    bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, dist);
                    bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, off);

                    // Shade hits
                    DispatchComputeIndirect(fg.cmd_buf(), pi_specular_shade_[6], fg.storages(), indir_args,
                                            sizeof(Ren::DispatchIndirectCommand), bindings, &uniform_params,
                                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
                });
            }
        }
    }

    FgImgRWHandle reproj_refl;
    FgImgRWHandle avg_refl;
    FgImgRWHandle sample_count, variance_temp;

    { // Denoiser reprojection
        auto &ssr_reproject = fg_builder_.AddNode("SPEC REPROJECT");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth, norm, velocity;
            FgImgROHandle depth_hist, norm_hist;
            FgImgROHandle refl_hist, sample_count_hist, variance_hist;
            FgImgROHandle refl;
            FgBufROHandle tile_list, indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_reprojected;
            FgImgRWHandle out_avg_refl;
            FgImgRWHandle out_variance, out_sample_count;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = ssr_reproject.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth = ssr_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm = ssr_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity = ssr_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist = ssr_reproject.AddHistoryTextureInput(OPAQUE_DEPTH_TEX, Stg::ComputeShader);
        data->norm_hist = ssr_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->refl_hist = ssr_reproject.AddHistoryTextureInput("GI Specular Filtered", Stg::ComputeShader);
        data->variance_hist = ssr_reproject.AddHistoryTextureInput(SPECULAR_VARIANCE_TEX, Stg::ComputeShader);
        data->refl = ssr_reproject.AddTextureInput(refl, Stg::ComputeShader);

        data->tile_list = ssr_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_reproject.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Reprojected reflections texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            reproj_refl = data->out_reprojected =
                ssr_reproject.AddStorageImageOutput("Spec Reprojected", desc, Stg::ComputeShader);
        }
        { // 8x8 average reflections texture
            FgImgDesc desc;
            desc.w = Ren::DivCeil(view_state_.ren_res[0], 8);
            desc.h = Ren::DivCeil(view_state_.ren_res[1], 8);
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            avg_refl = data->out_avg_refl =
                ssr_reproject.AddStorageImageOutput("Average Refl", desc, Stg::ComputeShader);
        }
        { // Variance
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            variance_temp = data->out_variance =
                ssr_reproject.AddStorageImageOutput("Variance Temp", desc, Stg::ComputeShader);
        }
        { // Sample count
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            sample_count = data->out_sample_count =
                ssr_reproject.AddStorageImageOutput("Sample Count", desc, Stg::ComputeShader);
        }

        data->sample_count_hist = ssr_reproject.AddHistoryTextureInput(data->out_sample_count, Stg::ComputeShader);

        ssr_reproject.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
            const Ren::ImageROHandle depth_hist = fg.AccessROImage(data->depth_hist);
            const Ren::ImageROHandle norm_hist = fg.AccessROImage(data->norm_hist);
            const Ren::ImageROHandle refl_hist = fg.AccessROImage(data->refl_hist);
            const Ren::ImageROHandle variance_hist = fg.AccessROImage(data->variance_hist);
            const Ren::ImageROHandle sample_count_hist = fg.AccessROImage(data->sample_count_hist);
            const Ren::ImageROHandle relf = fg.AccessROImage(data->refl);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_reprojected = fg.AccessRWImage(data->out_reprojected);
            const Ren::ImageRWHandle out_avg_refl = fg.AccessRWImage(data->out_avg_refl);
            const Ren::ImageRWHandle out_variance = fg.AccessRWImage(data->out_variance);
            const Ren::ImageRWHandle out_sample_count = fg.AccessRWImage(data->out_sample_count);

            { // Process tiles
                using namespace RTSpecularReproject;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity},
                                                 {Trg::TexSampled, DEPTH_HIST_TEX_SLOT, {depth_hist, 1}},
                                                 {Trg::TexSampled, NORM_HIST_TEX_SLOT, norm_hist},
                                                 {Trg::TexSampled, REFL_HIST_TEX_SLOT, refl_hist},
                                                 {Trg::TexSampled, VARIANCE_HIST_TEX_SLOT, variance_hist},
                                                 {Trg::TexSampled, SAMPLE_COUNT_HIST_TEX_SLOT, sample_count_hist},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, relf},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_REPROJECTED_IMG_SLOT, out_reprojected},
                                                 {Trg::ImageRW, OUT_AVG_REFL_IMG_SLOT, out_avg_refl},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance},
                                                 {Trg::ImageRW, OUT_SAMPLE_COUNT_IMG_SLOT, out_sample_count}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_reproject_, fg.storages(), indir_args,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_reprojected},
                                                 {Trg::ImageRW, OUT_AVG_RAD_IMG_SLOT, out_avg_refl},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[3], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    FgImgRWHandle prefiltered_refl;

    { // Denoiser prefilter
        auto &spec_prefilter = fg_builder_.AddNode("SPEC PREFILTER");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth, spec, norm;
            FgImgROHandle refl, avg_refl;
            FgImgROHandle sample_count, variance;
            FgBufROHandle tile_list, indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_refl;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = spec_prefilter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth = spec_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->spec = spec_prefilter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->norm = spec_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->refl = spec_prefilter.AddTextureInput(refl, Stg::ComputeShader);
        data->avg_refl = spec_prefilter.AddTextureInput(avg_refl, Stg::ComputeShader);
        data->sample_count = spec_prefilter.AddTextureInput(sample_count, Stg::ComputeShader);
        data->variance = spec_prefilter.AddTextureInput(variance_temp, Stg::ComputeShader);
        data->tile_list = spec_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = spec_prefilter.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Final reflection
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            prefiltered_refl = data->out_refl =
                spec_prefilter.AddStorageImageOutput("GI Specular 1", desc, Stg::ComputeShader);
        }

        spec_prefilter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle refl = fg.AccessROImage(data->refl);
            const Ren::ImageROHandle avg_refl = fg.AccessROImage(data->avg_refl);
            const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
            const Ren::ImageROHandle variance = fg.AccessROImage(data->variance);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->out_refl);

            { // Filter tiles
                using namespace RTSpecularFilter;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, {refl, nearest_sampler_}},
                                                 {Trg::TexSampled, AVG_REFL_TEX_SLOT, avg_refl},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                 {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl}};

                Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[0];
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_filter_[settings.taa_mode == eTAAMode::Static],
                                        fg.storages(), indir_args, data->indir_args_offset1, bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    FgImgRWHandle gi_specular, ssr_variance;

    { // Denoiser accumulation
        auto &spec_temporal = fg_builder_.AddNode("SPEC TEMPORAL");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle norm;
            FgImgROHandle refl, reproj_refl, avg_refl;
            FgImgROHandle variance, sample_count;
            FgBufROHandle tile_list, indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_refl;
            FgImgRWHandle out_variance;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = spec_temporal.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->norm = spec_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl = spec_temporal.AddTextureInput(avg_refl, Stg::ComputeShader);
        data->refl = spec_temporal.AddTextureInput(prefiltered_refl, Stg::ComputeShader);
        data->reproj_refl = spec_temporal.AddTextureInput(reproj_refl, Stg::ComputeShader);
        data->variance = spec_temporal.AddTextureInput(variance_temp, Stg::ComputeShader);
        data->sample_count = spec_temporal.AddTextureInput(sample_count, Stg::ComputeShader);
        data->tile_list = spec_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = spec_temporal.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        if (EnableBlur) {
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gi_specular = data->out_refl = spec_temporal.AddStorageImageOutput("GI Specular", desc, Stg::ComputeShader);
        } else {
            gi_specular = refl = data->out_refl = spec_temporal.AddStorageImageOutput(refl, Stg::ComputeShader);
        }
        { // Variance texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_variance = data->out_variance =
                spec_temporal.AddStorageImageOutput(SPECULAR_VARIANCE_TEX, desc, Stg::ComputeShader);
        }

        spec_temporal.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle avg_refl = fg.AccessROImage(data->avg_refl);
            const Ren::ImageROHandle refl = fg.AccessROImage(data->refl);
            const Ren::ImageROHandle reproj_refl = fg.AccessROImage(data->reproj_refl);
            const Ren::ImageROHandle variance = fg.AccessROImage(data->variance);
            const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->out_refl);
            const Ren::ImageRWHandle out_variance = fg.AccessRWImage(data->out_variance);

            { // Process tiles
                using namespace RTSpecularResolveTemporal;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, AVG_REFL_TEX_SLOT, avg_refl},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, refl},
                                                 {Trg::TexSampled, REPROJ_REFL_TEX_SLOT, reproj_refl},
                                                 {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_refl},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchComputeIndirect(fg.cmd_buf(), pi_specular_temporal_[settings.taa_mode == eTAAMode::Static],
                                        fg.storages(), indir_args, data->indir_args_offset1, bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[2], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    FgImgRWHandle refl_final = gi_specular;

    if (EnableBlur) {
        FgImgRWHandle gi_specular2;

        { // Denoiser blur
            auto &spec_filter = fg_builder_.AddNode("SPEC FILTER");

            struct PassData {
                FgBufROHandle shared_data;
                FgImgROHandle depth, spec, norm;
                FgImgROHandle refl, sample_count, variance;
                FgBufROHandle tile_list, indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgImgRWHandle out_refl;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->shared_data = spec_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth = spec_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec = spec_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm = spec_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->refl = spec_filter.AddTextureInput(gi_specular, Stg::ComputeShader);
            data->sample_count = spec_filter.AddTextureInput(sample_count, Stg::ComputeShader);
            data->variance = spec_filter.AddTextureInput(ssr_variance, Stg::ComputeShader);
            data->tile_list = spec_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = spec_filter.AddIndirectBufferInput(indir_disp);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final reflection
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular2 = data->out_refl =
                    spec_filter.AddStorageImageOutput("GI Specular 2", desc, Stg::ComputeShader);
            }

            spec_filter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
                const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
                const Ren::ImageROHandle refl = fg.AccessROImage(data->refl);
                const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
                const Ren::ImageROHandle variance = fg.AccessROImage(data->variance);
                const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

                const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->out_refl);

                { // Filter tiles
                    using namespace RTSpecularFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                     {Trg::TexSampled, REFL_TEX_SLOT, {refl, nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                     {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[0];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_specular_filter_[2], fg.storages(), indir_args,
                                            data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                            data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
            });
        }

        FgImgRWHandle gi_specular3;

        { // Denoiser blur 2
            auto &spec_post_filter = fg_builder_.AddNode("SPEC POST FILTER");

            struct PassData {
                FgBufROHandle shared_data;
                FgImgROHandle depth, spec, norm;
                FgImgROHandle refl, sample_count, variance;
                FgBufROHandle tile_list, indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgImgRWHandle out_refl;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->shared_data = spec_post_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth = spec_post_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec = spec_post_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm = spec_post_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->refl = spec_post_filter.AddTextureInput(gi_specular2, Stg::ComputeShader);
            data->sample_count = spec_post_filter.AddTextureInput(sample_count, Stg::ComputeShader);
            data->variance = spec_post_filter.AddTextureInput(ssr_variance, Stg::ComputeShader);
            data->tile_list = spec_post_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = spec_post_filter.AddIndirectBufferInput(indir_disp);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final reflection
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular3 = data->out_refl =
                    spec_post_filter.AddStorageImageOutput("GI Specular Filtered", desc, Stg::ComputeShader);
            }

            spec_post_filter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
                const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
                const Ren::ImageROHandle refl = fg.AccessROImage(data->refl);
                const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
                const Ren::ImageROHandle variance = fg.AccessROImage(data->variance);
                const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

                const Ren::ImageRWHandle out_refl = fg.AccessRWImage(data->out_refl);

                { // Filter tiles
                    using namespace RTSpecularFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                     {Trg::TexSampled, REFL_TEX_SLOT, {refl, nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                     {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[1];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_specular_filter_[3], fg.storages(), indir_args,
                                            data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                            data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
            });
        }

        FgImgRWHandle gi_specular4;

        { // Denoiser stabilization
            auto &spec_stabilization = fg_builder_.AddNode("SPEC STABILIZATION");

            struct PassData {
                FgImgROHandle depth, velocity;
                FgImgROHandle ssr, ssr_hist;
                FgImgRWHandle out_ssr;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->depth = spec_stabilization.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->velocity = spec_stabilization.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
            data->ssr = spec_stabilization.AddTextureInput(gi_specular3, Stg::ComputeShader);

            { // Final gi
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular4 = data->out_ssr =
                    spec_stabilization.AddStorageImageOutput("GI Specular 4", desc, Stg::ComputeShader);
            }

            data->ssr_hist = spec_stabilization.AddHistoryTextureInput(gi_specular4, Stg::ComputeShader);

            spec_stabilization.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTSpecularStabilization;

                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
                const Ren::ImageROHandle gi = fg.AccessROImage(data->ssr);
                const Ren::ImageROHandle gi_hist = fg.AccessROImage(data->ssr_hist);

                const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_ssr);

                const Ren::Binding bindings[] = {{Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity},
                                                 {Trg::TexSampled, SSR_TEX_SLOT, gi},
                                                 {Trg::TexSampled, SSR_HIST_TEX_SLOT, gi_hist},
                                                 {Trg::ImageRW, OUT_SSR_IMG_SLOT, out_gi}};

                const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                        (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchCompute(fg.cmd_buf(), pi_specular_stabilization_, fg.storages(), grp_count, bindings,
                                &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });

            if (EnableStabilization) {
                refl_final = gi_specular4;
            } else {
                refl_final = gi_specular3;
            }
        }
    }

    assert(deferred_shading);

    auto &spec_compose = fg_builder_.AddNode("SPEC COMPOSE");

    struct PassData {
        FgBufROHandle shared_data;
        FgImgROHandle depth;
        FgImgROHandle normal;
        FgImgROHandle albedo;
        FgImgROHandle spec;
        FgImgROHandle refl;
        FgImgROHandle ltc_luts;

        FgImgRWHandle output;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->shared_data = spec_compose.AddUniformBufferInput(common_buffers.shared_data,
                                                           Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    data->depth = spec_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
    data->normal = spec_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
    data->albedo = spec_compose.AddTextureInput(frame_textures.albedo, Stg::FragmentShader);
    data->spec = spec_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);

    if (debug_denoise) {
        data->refl = spec_compose.AddTextureInput(refl, Stg::FragmentShader);
    } else {
        data->refl = spec_compose.AddTextureInput(refl_final, Stg::FragmentShader);
    }
    data->ltc_luts = spec_compose.AddTextureInput(frame_textures.ltc_luts, Stg::FragmentShader);
    frame_textures.color = data->output = spec_compose.AddColorOutput(frame_textures.color);

    spec_compose.set_execute_cb([this, data](const FgContext &fg) {
        using namespace SSRCompose;

        const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
        const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
        const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
        const Ren::ImageROHandle albedo = fg.AccessROImage(data->albedo);
        const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
        const Ren::ImageROHandle refl = fg.AccessROImage(data->refl);
        const Ren::ImageROHandle ltc_luts = fg.AccessROImage(data->ltc_luts);

        const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

        Ren::RastState rast_state;
        rast_state.depth.test_enabled = false;
        rast_state.depth.write_enabled = false;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.blend.enabled = true;
        rast_state.blend.src_color = unsigned(Ren::eBlendFactor::One);
        rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::One);
        rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::One);
        rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

        rast_state.viewport[2] = view_state_.ren_res[0];
        rast_state.viewport[3] = view_state_.ren_res[1];

        { // compose reflections on top of clean buffer
            const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

            // TODO: get rid of global binding slots
            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), unif_sh_data},
                {Ren::eBindTarget::TexSampled, ALBEDO_TEX_SLOT, albedo},
                {Ren::eBindTarget::TexSampled, SPEC_TEX_SLOT, spec},
                {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                {Ren::eBindTarget::TexSampled, NORM_TEX_SLOT, normal},
                {Ren::eBindTarget::TexSampled, REFL_TEX_SLOT, refl},
                {Ren::eBindTarget::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts}};

            Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

            prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, {}, render_targets,
                                rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0,
                                fg.framebuffers());
        }
    });
}

void Eng::Renderer::AddLQSpecularPasses(const CommonBuffers &common_buffers, const FgImgROHandle depth_down_2x,
                                        FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgImgRWHandle ssr_temp1;
    { // Trace
        auto &ssr_trace = fg_builder_.AddNode("SSR TRACE");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle normal;
            FgImgROHandle depth_down_2x;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = ssr_trace.AddUniformBufferInput(common_buffers.shared_data,
                                                            Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        data->normal = ssr_trace.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->depth_down_2x = ssr_trace.AddTextureInput(depth_down_2x, Stg::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::RGB10_A2;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_temp1 = data->output = ssr_trace.AddColorOutput("SSR Temp 1", desc);
        }

        ssr_trace.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle depth_down_2x = fg.AccessROImage(data->depth_down_2x);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // screen space tracing
                const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {
                    {Trg::TexSampled, SSRTrace::DEPTH_TEX_SLOT, depth_down_2x},
                    {Trg::TexSampled, SSRTrace::NORM_TEX_SLOT, normal},
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), unif_sh_data}};

                SSRTrace::Params uniform_params;
                uniform_params.transform =
                    Ren::Vec4f{0.0f, 0.0f, float(view_state_.ren_res[0] / 2), float(view_state_.ren_res[1] / 2)};

                prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_ssr_prog_, {}, render_targets, rast_state,
                                    fg.rast_state(), bindings, &uniform_params, sizeof(SSRTrace::Params), 0,
                                    fg.framebuffers());
            }
        });
    }
    FgImgRWHandle ssr_temp2;
    { // Dilate
        auto &ssr_dilate = fg_builder_.AddNode("SSR DILATE");

        struct PassData {
            FgImgROHandle ssr;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->ssr = ssr_dilate.AddTextureInput(ssr_temp1, Stg::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::RGB10_A2;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_temp2 = data->output = ssr_dilate.AddColorOutput("Spec Temp 2", desc);
        }

        ssr_dilate.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle ssr = fg.AccessROImage(data->ssr);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // dilate ssr buffer
                const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::TexSampled, SSRDilate::SSR_TEX_SLOT, ssr}};

                SSRDilate::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, rast_state.viewport[2], rast_state.viewport[3]};

                prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_ssr_dilate_prog_, {}, render_targets,
                                    rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(SSRDilate::Params),
                                    0, fg.framebuffers());
            }
        });
    }
    /*{ // Compose
        auto &ssr_compose = fg_builder_.AddNode("SSR COMPOSE");

        auto *data = ssr_compose.AllocNodeData<ExSSRCompose::Args>();
        data->shared_data =
            ssr_compose.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
        data->cells = ssr_compose.AddStorageReadonlyInput(common_buffers.cells_res, Stg::FragmentShader);
        data->items = ssr_compose.AddStorageReadonlyInput(common_buffers.items_res, Stg::FragmentShader);

        data->depth = ssr_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->normal = ssr_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->spec = ssr_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);
        data->depth_down_2x = ssr_compose.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->down_buf_4x = ssr_compose.AddTextureInput(down_tex_4x_, Stg::FragmentShader);
        data->ssr = ssr_compose.AddTextureInput(ssr_temp2, Stg::FragmentShader);
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Stg::FragmentShader);

        frame_textures.color = data->output = ssr_compose.AddColorOutput(frame_textures.color);

        ex_ssr_compose_.Setup(fg_builder_, &view_state_, probe_storage, data);
        ssr_compose.set_executor(&ex_ssr_compose_);
    }*/
}