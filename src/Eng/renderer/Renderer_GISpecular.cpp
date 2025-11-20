#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/blit_ssr_compose_interface.h"
#include "shaders/blit_ssr_dilate_interface.h"
#include "shaders/blit_ssr_interface.h"
#include "shaders/ssr_classify_interface.h"
#include "shaders/ssr_filter_interface.h"
#include "shaders/ssr_reproject_interface.h"
#include "shaders/ssr_stabilization_interface.h"
#include "shaders/ssr_temporal_interface.h"
#include "shaders/ssr_trace_hq_interface.h"
#include "shaders/ssr_write_indirect_args_interface.h"
#include "shaders/tile_clear_interface.h"

#include "executors/ExRTReflections.h"

#include "Renderer_Names.h"

void Eng::Renderer::AddHQSpecularPasses(const bool deferred_shading, const bool debug_denoise,
                                        const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                        const AccelerationStructureData &acc_struct_data,
                                        const BindlessTextureData &bindless, const FgResRef depth_hierarchy,
                                        FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                                        FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    // Reflection settings
    const int SamplesPerQuad = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 4 : 1;
    static const bool VarianceGuided = true;
    static const bool EnableBlur = true;
    const bool EnableStabilization = false; //(settings.taa_mode != eTAAMode::Static);

    FgResRef ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &ssr_prepare = fg_builder_.AddNode("SSR PREPARE");

        struct PassData {
            FgResRef ray_counter;
            FgResRef ray_length_tex;
        };

        auto *data = ssr_prepare.AllocNodeData<PassData>();

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 8 * sizeof(uint32_t);

            ray_counter = data->ray_counter = ssr_prepare.AddTransferOutput("Ray Counter", desc);
        }

        ssr_prepare.set_execute_cb([data](FgContext &fg) {
            Ren::Buffer &ray_counter_buf = fg.AccessRWBuffer(data->ray_counter);

            ray_counter_buf.Fill(0, ray_counter_buf.size(), 0, fg.cmd_buf());
        });
    }

    const int tile_count = ((view_state_.ren_res[0] + 7) / 8) * ((view_state_.ren_res[1] + 7) / 8);

    FgResRef ray_list, tile_list;
    FgResRef refl_tex, noise_tex;

    { // Classify pixel quads
        auto &ssr_classify = fg_builder_.AddNode("SSR CLASSIFY");

        struct PassData {
            FgResRef depth;
            FgResRef specular;
            FgResRef normal;
            FgResRef variance_history;
            FgResRef bn_pmj_seq;
            FgResRef ray_counter;
            FgResRef ray_list;
            FgResRef tile_list;
            FgResRef out_refl_tex, out_noise_tex;
        };

        auto *data = ssr_classify.AllocNodeData<PassData>();
        data->depth = ssr_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->specular = ssr_classify.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->normal = ssr_classify.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->variance_history = ssr_classify.AddHistoryTextureInput(SPECULAR_VARIANCE_TEX, Stg::ComputeShader);
        data->bn_pmj_seq = ssr_classify.AddStorageReadonlyInput(bn_pmj_2D_64spp_seq_buf_, Stg::ComputeShader);
        ray_counter = data->ray_counter = ssr_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = ssr_classify.AddStorageOutput("Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = tile_count * sizeof(uint32_t);

            tile_list = data->tile_list = ssr_classify.AddStorageOutput("Tile List", desc, Stg::ComputeShader);
        }
        { // reflections texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            refl_tex = data->out_refl_tex = ssr_classify.AddStorageImageOutput("SSR Temp 2", desc, Stg::ComputeShader);
        }
        { // blue noise texture
            FgImgDesc desc;
            desc.w = desc.h = 128;
            desc.format = Ren::eFormat::RGBA8;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::Repeat;
            noise_tex = data->out_noise_tex = ssr_classify.AddStorageImageOutput("BN Tex", desc, Stg::ComputeShader);
        }

        ssr_classify.set_execute_cb([this, data, tile_count, SamplesPerQuad](FgContext &fg) {
            using namespace SSRClassifyTiles;

            const Ren::Image &depth_tex = fg.AccessROImage(data->depth);
            const Ren::Image &spec_tex = fg.AccessROImage(data->specular);
            const Ren::Image &norm_tex = fg.AccessROImage(data->normal);
            const Ren::Image &variance_tex = fg.AccessROImage(data->variance_history);
            const Ren::Buffer &bn_pmj_seq = fg.AccessROBuffer(data->bn_pmj_seq);

            Ren::Buffer &ray_counter_buf = fg.AccessRWBuffer(data->ray_counter);
            Ren::Buffer &ray_list_buf = fg.AccessRWBuffer(data->ray_list);
            Ren::Buffer &tile_list_buf = fg.AccessRWBuffer(data->tile_list);
            Ren::Image &refl_tex = fg.AccessRWImage(data->out_refl_tex);
            Ren::Image &noise_tex = fg.AccessRWImage(data->out_noise_tex);

            const Ren::Binding bindings[] = {
                {Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}}, {Trg::TexSampled, SPEC_TEX_SLOT, spec_tex},
                {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},        {Trg::TexSampled, VARIANCE_TEX_SLOT, variance_tex},
                {Trg::SBufRO, RAY_COUNTER_SLOT, ray_counter_buf},  {Trg::SBufRO, RAY_LIST_SLOT, ray_list_buf},
                {Trg::SBufRO, TILE_LIST_SLOT, tile_list_buf},      {Trg::UTBuf, BN_PMJ_SEQ_BUF_SLOT, bn_pmj_seq},
                {Trg::ImageRW, OUT_REFL_IMG_SLOT, refl_tex},       {Trg::ImageRW, OUT_NOISE_IMG_SLOT, noise_tex}};

            const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                    (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.samples_and_guided = Ren::Vec2u(SamplesPerQuad, VarianceGuided ? 1u : 0u);
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.clear = (view_state_.stochastic_lights_count > 0) ? 0.0f : 1.0f;
            uniform_params.tile_count = tile_count;

            DispatchCompute(*pi_ssr_classify_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            fg.descr_alloc(), fg.log());
        });
    }

    FgResRef indir_disp_buf;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("SSR INDIR ARGS");

        struct PassData {
            FgResRef ray_counter;
            FgResRef indir_disp_buf;
        };

        auto *data = write_indir.AllocNodeData<PassData>();
        ray_counter = data->ray_counter = write_indir.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // Indirect arguments
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 3 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp_buf = data->indir_disp_buf =
                write_indir.AddStorageOutput("Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](FgContext &fg) {
            using namespace SSRWriteIndirectArgs;

            Ren::Buffer &ray_counter_buf = fg.AccessRWBuffer(data->ray_counter);
            Ren::Buffer &indir_args = fg.AccessRWBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {{Trg::SBufRO, RAY_COUNTER_SLOT, ray_counter_buf},
                                             {Trg::SBufRW, INDIR_ARGS_SLOT, indir_args}};

            DispatchCompute(*pi_ssr_write_indirect_[0], Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0, fg.descr_alloc(),
                            fg.log());
        });
    }

    FgResRef ray_rt_list;

    if (settings.reflections_quality != eReflectionsQuality::Raytraced_High) {
        auto &ssr_trace_hq = fg_builder_.AddNode("SSR TRACE HQ");

        struct PassData {
            FgResRef noise_tex;
            FgResRef shared_data;
            FgResRef color_tex, normal_tex, depth_hierarchy;

            FgResRef albedo_tex, specular_tex, ltc_luts_tex, irradiance_tex, distance_tex, offset_tex;

            FgResRef in_ray_list, indir_args, inout_ray_counter;
            FgResRef refl_tex, out_ray_list;
        };

        auto *data = ssr_trace_hq.AllocNodeData<PassData>();
        data->noise_tex = ssr_trace_hq.AddTextureInput(noise_tex, Stg::ComputeShader);
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

        data->in_ray_list = ssr_trace_hq.AddStorageReadonlyInput(ray_list, Stg::ComputeShader);
        data->indir_args = ssr_trace_hq.AddIndirectBufferInput(indir_disp_buf);
        ray_counter = data->inout_ray_counter = ssr_trace_hq.AddStorageOutput(ray_counter, Stg::ComputeShader);
        refl_tex = data->refl_tex = ssr_trace_hq.AddStorageImageOutput(refl_tex, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list = ssr_trace_hq.AddStorageOutput("Ray RT List", desc, Stg::ComputeShader);
        }

        ssr_trace_hq.set_execute_cb([this, data](FgContext &fg) {
            using namespace SSRTraceHQ;

            const Ren::Image &noise_tex = fg.AccessROImage(data->noise_tex);
            const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            const Ren::Image &color_tex = fg.AccessROImage(data->color_tex);
            const Ren::Image &normal_tex = fg.AccessROImage(data->normal_tex);
            const Ren::Image &depth_hierarchy_tex = fg.AccessROImage(data->depth_hierarchy);
            const Ren::Buffer &in_ray_list_buf = fg.AccessROBuffer(data->in_ray_list);
            const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);

            const Ren::Image *albedo_tex = nullptr, *specular_tex = nullptr, *ltc_luts_tex = nullptr,
                               *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
            if (data->irradiance_tex) {
                albedo_tex = &fg.AccessROImage(data->albedo_tex);
                specular_tex = &fg.AccessROImage(data->specular_tex);
                ltc_luts_tex = &fg.AccessROImage(data->ltc_luts_tex);

                irr_tex = &fg.AccessROImage(data->irradiance_tex);
                dist_tex = &fg.AccessROImage(data->distance_tex);
                off_tex = &fg.AccessROImage(data->offset_tex);
            }

            Ren::Image &out_refl_tex = fg.AccessRWImage(data->refl_tex);
            Ren::Buffer &inout_ray_counter_buf = fg.AccessRWBuffer(data->inout_ray_counter);
            Ren::Buffer &out_ray_list_buf = fg.AccessRWBuffer(data->out_ray_list);

            Ren::SmallVector<Ren::Binding, 24> bindings = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                                           {Trg::TexSampled, DEPTH_TEX_SLOT, depth_hierarchy_tex},
                                                           {Trg::TexSampled, COLOR_TEX_SLOT, color_tex},
                                                           {Trg::TexSampled, NORM_TEX_SLOT, normal_tex},
                                                           {Trg::TexSampled, NOISE_TEX_SLOT, noise_tex},
                                                           {Trg::SBufRO, IN_RAY_LIST_SLOT, in_ray_list_buf},
                                                           {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_refl_tex},
                                                           {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, inout_ray_counter_buf},
                                                           {Trg::SBufRW, OUT_RAY_LIST_SLOT, out_ray_list_buf}};
            if (irr_tex) {
                bindings.emplace_back(Trg::TexSampled, ALBEDO_TEX_SLOT, *albedo_tex);
                bindings.emplace_back(Trg::TexSampled, SPEC_TEX_SLOT, *specular_tex);
                bindings.emplace_back(Trg::TexSampled, LTC_LUTS_TEX_SLOT, *ltc_luts_tex);

                bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, *irr_tex);
                bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, *dist_tex);
                bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, *off_tex);
            }

            Params uniform_params;
            uniform_params.resolution = Ren::Vec4u(view_state_.ren_res[0], view_state_.ren_res[1], 0, 0);

            DispatchComputeIndirect(*pi_ssr_trace_hq_[0][irr_tex != nullptr], indir_args_buf, 0, bindings,
                                    &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf &&
        int(settings.reflections_quality) >= int(eReflectionsQuality::Raytraced_Normal)) {
        FgResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("RT DISPATCH ARGS");

            struct PassData {
                FgResRef ray_counter;
                FgResRef indir_disp_buf;
            };

            auto *data = rt_disp_args.AllocNodeData<PassData>();
            ray_counter = data->ray_counter = rt_disp_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp_buf = data->indir_disp_buf =
                    rt_disp_args.AddStorageOutput("RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](FgContext &fg) {
                using namespace SSRWriteIndirectArgs;

                Ren::Buffer &ray_counter_buf = fg.AccessRWBuffer(data->ray_counter);
                Ren::Buffer &indir_disp_buf = fg.AccessRWBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter_buf},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp_buf}};

                Params params = {};
                params.counter_index = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 1 : 6;

                DispatchCompute(*pi_ssr_write_indirect_[1], Ren::Vec3u{1u, 1u, 1u}, bindings, &params, sizeof(params),
                                fg.descr_alloc(), fg.log());
            });
        }

        { // Trace reflection rays
            auto &rt_refl = fg_builder_.AddNode("RT REFLECTIONS");

            auto *data = rt_refl.AllocNodeData<ExRTReflections::Args>();

            const auto stage = Stg::ComputeShader;

            data->geo_data = rt_refl.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_refl.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_refl.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
            data->vtx_buf2 = rt_refl.AddStorageReadonlyInput(persistent_data.vertex_buf2, stage);
            data->ndx_buf = rt_refl.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
            data->shared_data = rt_refl.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->noise_tex = rt_refl.AddTextureInput(noise_tex, stage);
            data->depth_tex = rt_refl.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_refl.AddTextureInput(frame_textures.normal, stage);
            data->env_tex = rt_refl.AddTextureInput(frame_textures.envmap, stage);
            data->ray_counter = rt_refl.AddStorageReadonlyInput(ray_counter, stage);
            if (ray_rt_list) {
                data->ray_list = rt_refl.AddStorageReadonlyInput(ray_rt_list, stage);
            } else {
                data->ray_list = rt_refl.AddStorageReadonlyInput(ray_list, stage);
            }
            data->indir_args = rt_refl.AddIndirectBufferInput(indir_rt_disp_buf);
            data->tlas_buf = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf[int(eTLASIndex::Main)], stage);
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
            }

            data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

            data->probe_volume = &persistent_data.probe_volumes[0];
            if (settings.gi_quality != eGIQuality::Off) {
                data->irradiance_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
                data->distance_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_distance, stage);
                data->offset_tex = rt_refl.AddTextureInput(frame_textures.gi_cache_offset, stage);
            }

            if (persistent_data.stoch_lights_buf) {
                data->stoch_lights_buf =
                    rt_refl.AddStorageReadonlyInput(persistent_data.stoch_lights_buf, Stg::ComputeShader);
                data->light_nodes_buf =
                    rt_refl.AddStorageReadonlyInput(persistent_data.stoch_lights_nodes_buf, Stg::ComputeShader);
            }

            refl_tex = data->out_refl_tex[0] = rt_refl.AddStorageImageOutput(refl_tex, stage);

            data->layered = false;
            data->four_bounces = (settings.reflections_quality == eReflectionsQuality::Raytraced_High);

            rt_refl.make_executor<ExRTReflections>(&view_state_, &bindless, data, true);
        }
    }

    FgResRef reproj_refl_tex, avg_refl_tex;
    FgResRef sample_count_tex, variance_temp_tex;

    { // Denoiser reprojection
        auto &ssr_reproject = fg_builder_.AddNode("SSR REPROJECT");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex, norm_tex, velocity_tex;
            FgResRef depth_hist_tex, norm_hist_tex, refl_hist_tex, variance_hist_tex, sample_count_hist_tex;
            FgResRef refl_tex;
            FgResRef tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_reprojected_tex, out_avg_refl_tex;
            FgResRef out_variance_tex, out_sample_count_tex;
        };

        auto *data = ssr_reproject.AllocNodeData<PassData>();
        data->shared_data = ssr_reproject.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth_tex = ssr_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = ssr_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity_tex = ssr_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist_tex = ssr_reproject.AddHistoryTextureInput(OPAQUE_DEPTH_TEX, Stg::ComputeShader);
        data->norm_hist_tex = ssr_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->refl_hist_tex = ssr_reproject.AddHistoryTextureInput("GI Specular Filtered", Stg::ComputeShader);
        data->variance_hist_tex = ssr_reproject.AddHistoryTextureInput(SPECULAR_VARIANCE_TEX, Stg::ComputeShader);
        refl_tex = data->refl_tex = ssr_reproject.AddTextureInput(refl_tex, Stg::ComputeShader);

        data->tile_list = ssr_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Reprojected reflections texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            reproj_refl_tex = data->out_reprojected_tex =
                ssr_reproject.AddStorageImageOutput("SSR Reprojected", desc, Stg::ComputeShader);
        }
        { // 8x8 average reflections texture
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] + 7) / 8;
            desc.h = (view_state_.ren_res[1] + 7) / 8;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            avg_refl_tex = data->out_avg_refl_tex =
                ssr_reproject.AddStorageImageOutput("Average Refl", desc, Stg::ComputeShader);
        }
        { // Variance
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            variance_temp_tex = data->out_variance_tex =
                ssr_reproject.AddStorageImageOutput("Variance Temp", desc, Stg::ComputeShader);
        }
        { // Sample count
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            sample_count_tex = data->out_sample_count_tex =
                ssr_reproject.AddStorageImageOutput("Sample Count", desc, Stg::ComputeShader);
        }

        data->sample_count_hist_tex =
            ssr_reproject.AddHistoryTextureInput(data->out_sample_count_tex, Stg::ComputeShader);

        ssr_reproject.set_execute_cb([this, data, tile_count](FgContext &fg) {
            const Ren::Buffer &shared_data_buf = fg.AccessROBuffer(data->shared_data);
            const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
            const Ren::Image &norm_tex = fg.AccessROImage(data->norm_tex);
            const Ren::Image &velocity_tex = fg.AccessROImage(data->velocity_tex);
            const Ren::Image &depth_hist_tex = fg.AccessROImage(data->depth_hist_tex);
            const Ren::Image &norm_hist_tex = fg.AccessROImage(data->norm_hist_tex);
            const Ren::Image &refl_hist_tex = fg.AccessROImage(data->refl_hist_tex);
            const Ren::Image &variance_hist_tex = fg.AccessROImage(data->variance_hist_tex);
            const Ren::Image &sample_count_hist_tex = fg.AccessROImage(data->sample_count_hist_tex);
            const Ren::Image &relf_tex = fg.AccessROImage(data->refl_tex);
            const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(data->tile_list);
            const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);
            Ren::Image &out_reprojected_tex = fg.AccessRWImage(data->out_reprojected_tex);
            Ren::Image &out_avg_refl_tex = fg.AccessRWImage(data->out_avg_refl_tex);
            Ren::Image &out_variance_tex = fg.AccessRWImage(data->out_variance_tex);
            Ren::Image &out_sample_count_tex = fg.AccessRWImage(data->out_sample_count_tex);

            { // Process tiles
                using namespace SSRReproject;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data_buf},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity_tex},
                                                 {Trg::TexSampled, DEPTH_HIST_TEX_SLOT, {depth_hist_tex, 1}},
                                                 {Trg::TexSampled, NORM_HIST_TEX_SLOT, norm_hist_tex},
                                                 {Trg::TexSampled, REFL_HIST_TEX_SLOT, refl_hist_tex},
                                                 {Trg::TexSampled, VARIANCE_HIST_TEX_SLOT, variance_hist_tex},
                                                 {Trg::TexSampled, SAMPLE_COUNT_HIST_TEX_SLOT, sample_count_hist_tex},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, relf_tex},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_REPROJECTED_IMG_SLOT, out_reprojected_tex},
                                                 {Trg::ImageRW, OUT_AVG_REFL_IMG_SLOT, out_avg_refl_tex},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance_tex},
                                                 {Trg::ImageRW, OUT_SAMPLE_COUNT_IMG_SLOT, out_sample_count_tex}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchComputeIndirect(*pi_ssr_reproject_, indir_args_buf, data->indir_args_offset1, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_reprojected_tex},
                                                 {Trg::ImageRW, OUT_AVG_RAD_IMG_SLOT, out_avg_refl_tex},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance_tex}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[3], indir_args_buf, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
        });
    }

    FgResRef prefiltered_refl;

    { // Denoiser prefilter
        auto &ssr_prefilter = fg_builder_.AddNode("SSR PREFILTER");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex, spec_tex, norm_tex, refl_tex, avg_refl_tex;
            FgResRef sample_count_tex, variance_tex, tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_refl_tex;
        };

        auto *data = ssr_prefilter.AllocNodeData<PassData>();
        data->shared_data = ssr_prefilter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth_tex = ssr_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->spec_tex = ssr_prefilter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->norm_tex = ssr_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->refl_tex = ssr_prefilter.AddTextureInput(refl_tex, Stg::ComputeShader);
        data->avg_refl_tex = ssr_prefilter.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_prefilter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->variance_tex = ssr_prefilter.AddTextureInput(variance_temp_tex, Stg::ComputeShader);
        data->tile_list = ssr_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Final reflection
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            prefiltered_refl = data->out_refl_tex =
                ssr_prefilter.AddStorageImageOutput("GI Specular 1", desc, Stg::ComputeShader);
        }

        ssr_prefilter.set_execute_cb([this, data, tile_count](FgContext &fg) {
            const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
            const Ren::Image &spec_tex = fg.AccessROImage(data->spec_tex);
            const Ren::Image &norm_tex = fg.AccessROImage(data->norm_tex);
            const Ren::Image &refl_tex = fg.AccessROImage(data->refl_tex);
            const Ren::Image &avg_refl_tex = fg.AccessROImage(data->avg_refl_tex);
            const Ren::Image &sample_count_tex = fg.AccessROImage(data->sample_count_tex);
            const Ren::Image &variance_tex = fg.AccessROImage(data->variance_tex);
            const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(data->tile_list);
            const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);

            Ren::Image &out_refl_tex = fg.AccessRWImage(data->out_refl_tex);

            { // Filter tiles
                using namespace SSRFilter;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                 {Trg::TexSampled, SPEC_TEX_SLOT, spec_tex},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, {refl_tex, *nearest_sampler_}},
                                                 {Trg::TexSampled, AVG_REFL_TEX_SLOT, avg_refl_tex},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count_tex},
                                                 {Trg::TexSampled, VARIANCE_TEX_SLOT, variance_tex},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl_tex}};

                Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[0];
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                DispatchComputeIndirect(*pi_ssr_filter_[settings.taa_mode == eTAAMode::Static], indir_args_buf,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl_tex}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[0], indir_args_buf, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
        });
    }

    FgResRef gi_specular_tex, ssr_variance_tex;

    { // Denoiser accumulation
        auto &ssr_temporal = fg_builder_.AddNode("SSR TEMPORAL");

        struct PassData {
            FgResRef shared_data;
            FgResRef norm_tex, avg_refl_tex, refl_tex, reproj_refl_tex;
            FgResRef variance_tex, sample_count_tex, tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_refl_tex, out_variance_tex;
        };

        auto *data = ssr_temporal.AllocNodeData<PassData>();
        data->shared_data = ssr_temporal.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->norm_tex = ssr_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl_tex = ssr_temporal.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->refl_tex = ssr_temporal.AddTextureInput(prefiltered_refl, Stg::ComputeShader);
        data->reproj_refl_tex = ssr_temporal.AddTextureInput(reproj_refl_tex, Stg::ComputeShader);
        data->variance_tex = ssr_temporal.AddTextureInput(variance_temp_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_temporal.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->tile_list = ssr_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        if (EnableBlur) {
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gi_specular_tex = data->out_refl_tex =
                ssr_temporal.AddStorageImageOutput("GI Specular", desc, Stg::ComputeShader);
        } else {
            gi_specular_tex = refl_tex = data->out_refl_tex =
                ssr_temporal.AddStorageImageOutput(refl_tex, Stg::ComputeShader);
        }
        { // Variance texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_variance_tex = data->out_variance_tex =
                ssr_temporal.AddStorageImageOutput(SPECULAR_VARIANCE_TEX, desc, Stg::ComputeShader);
        }

        ssr_temporal.set_execute_cb([this, data, tile_count](FgContext &fg) {
            const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            const Ren::Image &norm_tex = fg.AccessROImage(data->norm_tex);
            const Ren::Image &avg_refl_tex = fg.AccessROImage(data->avg_refl_tex);
            const Ren::Image &refl_tex = fg.AccessROImage(data->refl_tex);
            const Ren::Image &reproj_refl_tex = fg.AccessROImage(data->reproj_refl_tex);
            const Ren::Image &variance_tex = fg.AccessROImage(data->variance_tex);
            const Ren::Image &sample_count_tex = fg.AccessROImage(data->sample_count_tex);
            const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(data->tile_list);
            const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);

            Ren::Image &out_refl_tex = fg.AccessRWImage(data->out_refl_tex);
            Ren::Image &out_variance_tex = fg.AccessRWImage(data->out_variance_tex);

            { // Process tiles
                using namespace SSRResolveTemporal;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},
                                                 {Trg::TexSampled, AVG_REFL_TEX_SLOT, avg_refl_tex},
                                                 {Trg::TexSampled, REFL_TEX_SLOT, refl_tex},
                                                 {Trg::TexSampled, REPROJ_REFL_TEX_SLOT, reproj_refl_tex},
                                                 {Trg::TexSampled, VARIANCE_TEX_SLOT, variance_tex},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count_tex},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_REFL_IMG_SLOT, out_refl_tex},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance_tex}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchComputeIndirect(*pi_ssr_temporal_[settings.taa_mode == eTAAMode::Static], indir_args_buf,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl_tex},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance_tex}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[2], indir_args_buf, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
        });
    }

    FgResRef refl_final = gi_specular_tex;

    if (EnableBlur) {
        FgResRef gi_specular2_tex;

        { // Denoiser blur
            auto &ssr_filter = fg_builder_.AddNode("SSR FILTER");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, refl_tex;
                FgResRef sample_count_tex, variance_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgResRef out_refl_tex;
            };

            auto *data = ssr_filter.AllocNodeData<PassData>();
            data->shared_data = ssr_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth_tex = ssr_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec_tex = ssr_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm_tex = ssr_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->refl_tex = ssr_filter.AddTextureInput(gi_specular_tex, Stg::ComputeShader);
            data->sample_count_tex = ssr_filter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->variance_tex = ssr_filter.AddTextureInput(ssr_variance_tex, Stg::ComputeShader);
            data->tile_list = ssr_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = ssr_filter.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final reflection
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular2_tex = data->out_refl_tex =
                    ssr_filter.AddStorageImageOutput("GI Specular 2", desc, Stg::ComputeShader);
            }

            ssr_filter.set_execute_cb([this, data, tile_count](FgContext &fg) {
                const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
                const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
                const Ren::Image &spec_tex = fg.AccessROImage(data->spec_tex);
                const Ren::Image &norm_tex = fg.AccessROImage(data->norm_tex);
                const Ren::Image &refl_tex = fg.AccessROImage(data->refl_tex);
                const Ren::Image &sample_count_tex = fg.AccessROImage(data->sample_count_tex);
                const Ren::Image &variance_tex = fg.AccessROImage(data->variance_tex);
                const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(data->tile_list);
                const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);

                Ren::Image &out_refl_tex = fg.AccessRWImage(data->out_refl_tex);

                { // Filter tiles
                    using namespace SSRFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec_tex},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},
                                                     {Trg::TexSampled, REFL_TEX_SLOT, {refl_tex, *nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count_tex},
                                                     {Trg::TexSampled, VARIANCE_TEX_SLOT, variance_tex},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl_tex}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[0];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(*pi_ssr_filter_[2], indir_args_buf, data->indir_args_offset1, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl_tex}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(*pi_tile_clear_[0], indir_args_buf, data->indir_args_offset2, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
                }
            });
        }

        FgResRef gi_specular3_tex;

        { // Denoiser blur 2
            auto &ssr_post_filter = fg_builder_.AddNode("SSR POST FILTER");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, refl_tex;
                FgResRef sample_count_tex, variance_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgResRef out_refl_tex;
            };

            auto *data = ssr_post_filter.AllocNodeData<PassData>();
            data->shared_data = ssr_post_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth_tex = ssr_post_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec_tex = ssr_post_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm_tex = ssr_post_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->refl_tex = ssr_post_filter.AddTextureInput(gi_specular2_tex, Stg::ComputeShader);
            data->sample_count_tex = ssr_post_filter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->variance_tex = ssr_post_filter.AddTextureInput(ssr_variance_tex, Stg::ComputeShader);
            data->tile_list = ssr_post_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = ssr_post_filter.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final reflection
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular3_tex = data->out_refl_tex =
                    ssr_post_filter.AddStorageImageOutput("GI Specular Filtered", desc, Stg::ComputeShader);
            }

            ssr_post_filter.set_execute_cb([this, data, tile_count](FgContext &fg) {
                const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
                const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
                const Ren::Image &spec_tex = fg.AccessROImage(data->spec_tex);
                const Ren::Image &norm_tex = fg.AccessROImage(data->norm_tex);
                const Ren::Image &refl_tex = fg.AccessROImage(data->refl_tex);
                const Ren::Image &sample_count_tex = fg.AccessROImage(data->sample_count_tex);
                const Ren::Image &variance_tex = fg.AccessROImage(data->variance_tex);
                const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(data->tile_list);
                const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(data->indir_args);

                Ren::Image &out_refl_tex = fg.AccessRWImage(data->out_refl_tex);

                { // Filter tiles
                    using namespace SSRFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec_tex},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm_tex},
                                                     {Trg::TexSampled, REFL_TEX_SLOT, {refl_tex, *nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count_tex},
                                                     {Trg::TexSampled, VARIANCE_TEX_SLOT, variance_tex},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_refl_tex}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[1];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(*pi_ssr_filter_[3], indir_args_buf, data->indir_args_offset1, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list_buf},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_refl_tex}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(*pi_tile_clear_[0], indir_args_buf, data->indir_args_offset2, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
                }
            });
        }

        FgResRef gi_specular4_tex;

        { // Denoiser stabilization
            auto &ssr_stabilization = fg_builder_.AddNode("SSR STABILIZATION");

            struct PassData {
                FgResRef depth_tex, velocity_tex, ssr_tex, ssr_hist_tex;
                FgResRef out_ssr_tex;
            };

            auto *data = ssr_stabilization.AllocNodeData<PassData>();
            data->depth_tex = ssr_stabilization.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->velocity_tex = ssr_stabilization.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
            data->ssr_tex = ssr_stabilization.AddTextureInput(gi_specular3_tex, Stg::ComputeShader);

            { // Final gi
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_specular4_tex = data->out_ssr_tex =
                    ssr_stabilization.AddStorageImageOutput("GI Specular 4", desc, Stg::ComputeShader);
            }

            data->ssr_hist_tex = ssr_stabilization.AddHistoryTextureInput(gi_specular4_tex, Stg::ComputeShader);

            ssr_stabilization.set_execute_cb([this, data](FgContext &fg) {
                using namespace SSRStabilization;

                const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
                const Ren::Image &velocity_tex = fg.AccessROImage(data->velocity_tex);
                const Ren::Image &gi_tex = fg.AccessROImage(data->ssr_tex);
                const Ren::Image &gi_hist_tex = fg.AccessROImage(data->ssr_hist_tex);

                Ren::Image &out_gi_tex = fg.AccessRWImage(data->out_ssr_tex);

                const Ren::Binding bindings[] = {{Trg::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity_tex},
                                                 {Trg::TexSampled, SSR_TEX_SLOT, gi_tex},
                                                 {Trg::TexSampled, SSR_HIST_TEX_SLOT, gi_hist_tex},
                                                 {Trg::ImageRW, OUT_SSR_IMG_SLOT, out_gi_tex}};

                const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                        (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchCompute(*pi_ssr_stabilization_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                fg.descr_alloc(), fg.log());
            });

            if (EnableStabilization) {
                refl_final = gi_specular4_tex;
            } else {
                refl_final = gi_specular3_tex;
            }
        }
    }

    assert(deferred_shading);

    auto &ssr_compose = fg_builder_.AddNode("SSR COMPOSE");

    struct PassData {
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef normal_tex;
        FgResRef albedo_tex;
        FgResRef spec_tex;
        FgResRef refl_tex;
        FgResRef ltc_luts;

        FgResRef output_tex;
    };

    auto *data = ssr_compose.AllocNodeData<PassData>();

    data->shared_data = ssr_compose.AddUniformBufferInput(common_buffers.shared_data,
                                                          Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
    data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
    data->albedo_tex = ssr_compose.AddTextureInput(frame_textures.albedo, Stg::FragmentShader);
    data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);

    if (debug_denoise) {
        data->refl_tex = ssr_compose.AddTextureInput(refl_tex, Stg::FragmentShader);
    } else {
        data->refl_tex = ssr_compose.AddTextureInput(refl_final, Stg::FragmentShader);
    }
    data->ltc_luts = ssr_compose.AddTextureInput(ltc_luts_, Stg::FragmentShader);
    frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

    ssr_compose.set_execute_cb([this, data](FgContext &fg) {
        using namespace SSRCompose;

        const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
        const Ren::Image &depth_tex = fg.AccessROImage(data->depth_tex);
        const Ren::Image &normal_tex = fg.AccessROImage(data->normal_tex);
        const Ren::Image &albedo_tex = fg.AccessROImage(data->albedo_tex);
        const Ren::Image &spec_tex = fg.AccessROImage(data->spec_tex);
        const Ren::Image &refl_tex = fg.AccessROImage(data->refl_tex);
        const Ren::Image &ltc_luts = fg.AccessROImage(data->ltc_luts);

        Ren::WeakImgRef output_tex = fg.AccessRWImageRef(data->output_tex);

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
            const Ren::RenderTarget render_targets[] = {{output_tex, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

            // TODO: get rid of global binding slots
            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), unif_sh_data_buf},
                {Ren::eBindTarget::TexSampled, ALBEDO_TEX_SLOT, albedo_tex},
                {Ren::eBindTarget::TexSampled, SPEC_TEX_SLOT, spec_tex},
                {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                {Ren::eBindTarget::TexSampled, NORM_TEX_SLOT, normal_tex},
                {Ren::eBindTarget::TexSampled, REFL_TEX_SLOT, refl_tex},
                {Ren::eBindTarget::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts}};

            Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, {}, render_targets, rast_state,
                                fg.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
        }
    });
}

void Eng::Renderer::AddLQSpecularPasses(const CommonBuffers &common_buffers, const FgResRef depth_down_2x,
                                        FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgResRef ssr_temp1;
    { // Trace
        auto &ssr_trace = fg_builder_.AddNode("SSR TRACE");

        struct PassData {
            FgResRef shared_data;
            FgResRef normal_tex;
            FgResRef depth_down_2x_tex;
            FgResRef output_tex;
        };

        auto *data = ssr_trace.AllocNodeData<PassData>();
        data->shared_data = ssr_trace.AddUniformBufferInput(common_buffers.shared_data,
                                                            Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
        data->normal_tex = ssr_trace.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->depth_down_2x_tex = ssr_trace.AddTextureInput(depth_down_2x, Stg::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::RGB10_A2;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_temp1 = data->output_tex = ssr_trace.AddColorOutput("SSR Temp 1", desc);
        }

        ssr_trace.set_execute_cb([this, data](FgContext &fg) {
            const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(data->shared_data);
            const Ren::Image &normal_tex = fg.AccessROImage(data->normal_tex);
            const Ren::Image &depth_down_2x_tex = fg.AccessROImage(data->depth_down_2x_tex);

            Ren::WeakImgRef output_tex = fg.AccessRWImageRef(data->output_tex);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // screen space tracing
                const Ren::RenderTarget render_targets[] = {{output_tex, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {
                    {Trg::TexSampled, SSRTrace::DEPTH_TEX_SLOT, depth_down_2x_tex},
                    {Trg::TexSampled, SSRTrace::NORM_TEX_SLOT, normal_tex},
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), unif_sh_data_buf}};

                SSRTrace::Params uniform_params;
                uniform_params.transform =
                    Ren::Vec4f{0.0f, 0.0f, float(view_state_.ren_res[0] / 2), float(view_state_.ren_res[1] / 2)};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_prog_, {}, render_targets, rast_state,
                                    fg.rast_state(), bindings, &uniform_params, sizeof(SSRTrace::Params), 0);
            }
        });
    }
    FgResRef ssr_temp2;
    { // Dilate
        auto &ssr_dilate = fg_builder_.AddNode("SSR DILATE");

        struct PassData {
            FgResRef ssr_tex;
            FgResRef output_tex;
        };

        auto *data = ssr_dilate.AllocNodeData<PassData>();
        data->ssr_tex = ssr_dilate.AddTextureInput(ssr_temp1, Stg::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::RGB10_A2;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssr_temp2 = data->output_tex = ssr_dilate.AddColorOutput("SSR Temp 2", desc);
        }

        ssr_dilate.set_execute_cb([this, data](FgContext &fg) {
            const Ren::Image &ssr_tex = fg.AccessROImage(data->ssr_tex);

            Ren::WeakImgRef output_tex = fg.AccessRWImageRef(data->output_tex);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // dilate ssr buffer
                const Ren::RenderTarget render_targets[] = {{output_tex, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::TexSampled, SSRDilate::SSR_TEX_SLOT, ssr_tex}};

                SSRDilate::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, rast_state.viewport[2], rast_state.viewport[3]};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_dilate_prog_, {}, render_targets, rast_state,
                                    fg.rast_state(), bindings, &uniform_params, sizeof(SSRDilate::Params), 0);
            }
        });
    }
    /*{ // Compose
        auto &ssr_compose = fg_builder_.AddNode("SSR COMPOSE");

        auto *data = ssr_compose.AllocNodeData<ExSSRCompose::Args>();
        data->shared_data =
            ssr_compose.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
        data->cells_buf = ssr_compose.AddStorageReadonlyInput(common_buffers.cells_res, Stg::FragmentShader);
        data->items_buf = ssr_compose.AddStorageReadonlyInput(common_buffers.items_res, Stg::FragmentShader);

        data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);
        data->depth_down_2x_tex = ssr_compose.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->down_buf_4x_tex = ssr_compose.AddTextureInput(down_tex_4x_, Stg::FragmentShader);
        data->ssr_tex = ssr_compose.AddTextureInput(ssr_temp2, Stg::FragmentShader);
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Stg::FragmentShader);

        frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

        ex_ssr_compose_.Setup(fg_builder_, &view_state_, probe_storage, data);
        ssr_compose.set_executor(&ex_ssr_compose_);
    }*/
}