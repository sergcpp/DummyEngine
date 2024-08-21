#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/blit_ssr_compose_new_interface.h"
#include "shaders/blit_ssr_dilate_interface.h"
#include "shaders/blit_ssr_interface.h"
#include "shaders/ssr_blur_interface.h"
#include "shaders/ssr_classify_interface.h"
#include "shaders/ssr_prefilter_interface.h"
#include "shaders/ssr_reproject_interface.h"
#include "shaders/ssr_stabilization_interface.h"
#include "shaders/ssr_temporal_interface.h"
#include "shaders/ssr_trace_hq_interface.h"
#include "shaders/ssr_write_indir_rt_dispatch_interface.h"
#include "shaders/ssr_write_indirect_args_interface.h"

void Eng::Renderer::AddHQSpecularPasses(const bool deferred_shading, const bool debug_denoise,
                                        const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                        const PersistentGpuData &persistent_data,
                                        const AccelerationStructureData &acc_struct_data,
                                        const BindlessTextureData &bindless, const FgResRef depth_hierarchy,
                                        FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                                        FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    // Reflection settings
    const int SamplesPerQuad = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 4 : 1;
    static const bool VarianceGuided = true;
    static const bool EnableBlur = true;
    const bool EnableStabilization = (settings.taa_mode != eTAAMode::Static);

    FgResRef ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &ssr_prepare = fg_builder_.AddNode("SSR PREPARE");

        struct PassData {
            FgResRef ray_counter;
            FgResRef ray_length_tex;
        };

        auto *data = ssr_prepare.AllocNodeData<PassData>();

        { // ray counter
            FgBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = 6 * sizeof(uint32_t);

            ray_counter = data->ray_counter = ssr_prepare.AddTransferOutput("Ray Counter", desc);
        }

        ssr_prepare.set_execute_cb([data](FgBuilder &builder) {
            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);

            Ren::Context &ctx = builder.ctx();
            ray_counter_buf.ref->Fill(0, ray_counter_buf.ref->size(), 0, ctx.current_cmd_buf());
        });
    }

    FgResRef ray_list, tile_list;
    FgResRef refl_tex, noise_tex;

    { // Classify pixel quads
        auto &ssr_classify = fg_builder_.AddNode("SSR CLASSIFY");

        struct PassData {
            FgResRef depth;
            FgResRef specular;
            FgResRef normal;
            FgResRef variance_history;
            FgResRef sobol, scrambling_tile, ranking_tile;
            FgResRef ray_counter;
            FgResRef ray_list;
            FgResRef tile_list;
            FgResRef out_refl_tex, out_noise_tex;
        };

        auto *data = ssr_classify.AllocNodeData<PassData>();
        data->depth = ssr_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->specular = ssr_classify.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->normal = ssr_classify.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->variance_history = ssr_classify.AddHistoryTextureInput("Variance", Stg::ComputeShader);
        data->sobol = ssr_classify.AddStorageReadonlyInput(sobol_seq_buf_, Stg::ComputeShader);
        data->scrambling_tile = ssr_classify.AddStorageReadonlyInput(scrambling_tile_buf_, Stg::ComputeShader);
        data->ranking_tile = ssr_classify.AddStorageReadonlyInput(ranking_tile_buf_, Stg::ComputeShader);
        ray_counter = data->ray_counter = ssr_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = ssr_classify.AddStorageOutput("Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            FgBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 7) / 8) * sizeof(uint32_t);

            tile_list = data->tile_list = ssr_classify.AddStorageOutput("Tile List", desc, Stg::ComputeShader);
        }
        { // reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            refl_tex = data->out_refl_tex =
                ssr_classify.AddStorageImageOutput("SSR Temp 2", params, Stg::ComputeShader);
        }
        { // blue noise texture
            Ren::Tex2DParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RawRGBA8888;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex =
                ssr_classify.AddStorageImageOutput("Blue Noise Tex", params, Stg::ComputeShader);
        }

        ssr_classify.set_execute_cb([this, data, SamplesPerQuad](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &spec_tex = builder.GetReadTexture(data->specular);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_history);
            FgAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            FgAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            FgAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            FgAllocBuf &ray_list_buf = builder.GetWriteBuffer(data->ray_list);
            FgAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            FgAllocTex &refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            FgAllocTex &noise_tex = builder.GetWriteTexture(data->out_noise_tex);

            // Initialize texel buffers if needed
            if (!sobol_buf.tbos[0]) {
                sobol_buf.tbos[0] = ctx_.CreateTexture1D("SobolSequenceTex", sobol_buf.ref, Ren::eTexFormat::RawR32UI,
                                                         0, sobol_buf.ref->size());
            }
            if (!scrambling_tile_buf.tbos[0]) {
                scrambling_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("ScramblingTile32SppTex", scrambling_tile_buf.ref, Ren::eTexFormat::RawR32UI,
                                         0, scrambling_tile_buf.ref->size());
            }
            if (!ranking_tile_buf.tbos[0]) {
                ranking_tile_buf.tbos[0] =
                    ctx_.CreateTexture1D("RankingTile32SppTex", ranking_tile_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                         ranking_tile_buf.ref->size());
            }

            const Ren::Binding bindings[] = {
                {Trg::Tex2DSampled, SSRClassifyTiles::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, SSRClassifyTiles::SPEC_TEX_SLOT, *spec_tex.ref},
                {Trg::Tex2DSampled, SSRClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, SSRClassifyTiles::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Trg::SBufRO, SSRClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Trg::SBufRO, SSRClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
                {Trg::SBufRO, SSRClassifyTiles::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Trg::UTBuf, SSRClassifyTiles::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Trg::UTBuf, SSRClassifyTiles::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Trg::UTBuf, SSRClassifyTiles::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Trg::Image2D, SSRClassifyTiles::REFL_IMG_SLOT, *refl_tex.ref},
                {Trg::Image2D, SSRClassifyTiles::NOISE_IMG_SLOT, *noise_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + SSRClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) /
                               SSRClassifyTiles::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + SSRClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) /
                               SSRClassifyTiles::LOCAL_GROUP_SIZE_Y,
                           1u};

            SSRClassifyTiles::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.samples_and_guided = Ren::Vec2u{uint32_t(SamplesPerQuad), VarianceGuided ? 1u : 0u};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.clear = (view_state_.stochastic_lights_count > 0) ? 0.0f : 1.0f;

            Ren::DispatchCompute(pi_ssr_classify_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
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
            desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp_buf = data->indir_disp_buf =
                write_indir.AddStorageOutput("Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            FgAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {
                {Trg::SBufRO, SSRWriteIndirectArgs::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Trg::SBufRW, SSRWriteIndirectArgs::INDIR_ARGS_SLOT, *indir_args.ref}};

            Ren::DispatchCompute(pi_ssr_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
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
        data->shared_data = ssr_trace_hq.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
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
            FgBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list = ssr_trace_hq.AddStorageOutput("Ray RT List", desc, Stg::ComputeShader);
        }

        ssr_trace_hq.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &noise_tex = builder.GetReadTexture(data->noise_tex);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &color_tex = builder.GetReadTexture(data->color_tex);
            FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            FgAllocTex &depth_hierarchy_tex = builder.GetReadTexture(data->depth_hierarchy);
            FgAllocBuf &in_ray_list_buf = builder.GetReadBuffer(data->in_ray_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex *albedo_tex = nullptr, *specular_tex = nullptr, *ltc_luts_tex = nullptr,
                       *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
            if (data->irradiance_tex) {
                albedo_tex = &builder.GetReadTexture(data->albedo_tex);
                specular_tex = &builder.GetReadTexture(data->specular_tex);
                ltc_luts_tex = &builder.GetReadTexture(data->ltc_luts_tex);

                irradiance_tex = &builder.GetReadTexture(data->irradiance_tex);
                distance_tex = &builder.GetReadTexture(data->distance_tex);
                offset_tex = &builder.GetReadTexture(data->offset_tex);
            }

            FgAllocTex &out_refl_tex = builder.GetWriteTexture(data->refl_tex);
            FgAllocBuf &inout_ray_counter_buf = builder.GetWriteBuffer(data->inout_ray_counter);
            FgAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(data->out_ray_list);

            Ren::SmallVector<Ren::Binding, 24> bindings = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, SSRTraceHQ::DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                {Trg::Tex2DSampled, SSRTraceHQ::COLOR_TEX_SLOT, *color_tex.ref},
                {Trg::Tex2DSampled, SSRTraceHQ::NORM_TEX_SLOT, *normal_tex.ref},
                {Trg::Tex2DSampled, SSRTraceHQ::NOISE_TEX_SLOT, *noise_tex.ref},
                {Trg::SBufRO, SSRTraceHQ::IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                {Trg::Image2D, SSRTraceHQ::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Trg::SBufRW, SSRTraceHQ::INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                {Trg::SBufRW, SSRTraceHQ::OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};
            if (irradiance_tex) {
                bindings.emplace_back(Trg::Tex2DSampled, SSRTraceHQ::ALBEDO_TEX_SLOT, *albedo_tex->ref);
                bindings.emplace_back(Trg::Tex2DSampled, SSRTraceHQ::SPEC_TEX_SLOT, *specular_tex->ref);
                bindings.emplace_back(Trg::Tex2DSampled, SSRTraceHQ::LTC_LUTS_TEX_SLOT, *ltc_luts_tex->ref);

                bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, SSRTraceHQ::IRRADIANCE_TEX_SLOT,
                                      *irradiance_tex->arr);
                bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, SSRTraceHQ::DISTANCE_TEX_SLOT,
                                      *distance_tex->arr);
                bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, SSRTraceHQ::OFFSET_TEX_SLOT,
                                      *offset_tex->arr);
            }

            SSRTraceHQ::Params uniform_params;
            uniform_params.resolution =
                Ren::Vec4u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1]), 0, 0};

            Ren::DispatchComputeIndirect(pi_ssr_trace_hq_[0][irradiance_tex != nullptr], *indir_args_buf.ref, 0,
                                         bindings, &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf && frame_textures.envmap &&
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

            rt_disp_args.set_execute_cb([this, data](FgBuilder &builder) {
                FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                FgAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {
                    {Trg::SBufRW, SSRWriteIndirRTDispatch::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                    {Trg::SBufRW, SSRWriteIndirRTDispatch::INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                SSRWriteIndirRTDispatch::Params params = {};
                params.counter_index = (settings.reflections_quality == eReflectionsQuality::Raytraced_High) ? 1 : 4;

                Ren::DispatchCompute(pi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, &params, sizeof(params),
                                     builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace reflection rays
            auto &rt_refl = fg_builder_.AddNode("RT REFLECTIONS");

            auto *data = rt_refl.AllocNodeData<ExRTReflections::Args>();
            data->four_bounces = (settings.reflections_quality == eReflectionsQuality::Raytraced_High);

            const auto stage = Stg::ComputeShader;

            data->geo_data = rt_refl.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_refl.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
            data->vtx_buf2 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
            data->ndx_buf = rt_refl.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
            data->shared_data = rt_refl.AddUniformBufferInput(common_buffers.shared_data_res, stage);
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
            data->tlas_buf = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
            data->lights_buf = rt_refl.AddStorageReadonlyInput(common_buffers.lights_res, stage);
            data->shadowmap_tex = rt_refl.AddTextureInput(shadow_map_tex_, stage);
            data->ltc_luts_tex = rt_refl.AddTextureInput(ltc_luts_, stage);
            data->cells_buf = rt_refl.AddStorageReadonlyInput(common_buffers.rt_cells_res, stage);
            data->items_buf = rt_refl.AddStorageReadonlyInput(common_buffers.rt_items_res, stage);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx_buf =
                    rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
                data->swrt.meshes_buf = rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_meshes_buf, stage);
                data->swrt.mesh_instances_buf = rt_refl.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(USE_GL_RENDER)
                data->swrt.textures_buf = rt_refl.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
            }

            data->dummy_black = rt_refl.AddTextureInput(dummy_black_, stage);

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

            ex_rt_reflections_.Setup(fg_builder_, &view_state_, &bindless, data);
            rt_refl.set_executor(&ex_rt_reflections_);
        }
    }

    FgResRef reproj_refl_tex, avg_refl_tex;
    FgResRef depth_hist_tex, sample_count_tex, variance_temp_tex;

    { // Denoiser reprojection
        auto &ssr_reproject = fg_builder_.AddNode("SSR REPROJECT");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex, norm_tex, velocity_tex;
            FgResRef depth_hist_tex, norm_hist_tex, refl_hist_tex, variance_hist_tex, sample_count_hist_tex;
            FgResRef refl_tex;
            FgResRef exposure_tex;
            FgResRef tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset = 0;
            FgResRef out_reprojected_tex, out_avg_refl_tex;
            FgResRef out_variance_tex, out_sample_count_tex;
        };

        auto *data = ssr_reproject.AllocNodeData<PassData>();
        data->shared_data = ssr_reproject.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        data->depth_tex = ssr_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = ssr_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity_tex = ssr_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist_tex = ssr_reproject.AddHistoryTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_hist_tex = ssr_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        /*if (EnableBlur) {
            data->refl_hist_tex = ssr_reproject.AddHistoryTextureInput("GI Specular 3", Stg::ComputeShader);
        } else {
            data->refl_hist_tex = ssr_reproject.AddHistoryTextureInput("GI Specular", Stg::ComputeShader);
        }*/
        data->refl_hist_tex = ssr_reproject.AddHistoryTextureInput(refl_tex, Stg::ComputeShader);
        data->variance_hist_tex = ssr_reproject.AddHistoryTextureInput("Variance", Stg::ComputeShader);
        refl_tex = data->refl_tex = ssr_reproject.AddTextureInput(refl_tex, Stg::ComputeShader);
        data->exposure_tex = ssr_reproject.AddHistoryTextureInput("Exposure", Stg::ComputeShader);

        data->tile_list = ssr_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reprojected reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            reproj_refl_tex = data->out_reprojected_tex =
                ssr_reproject.AddStorageImageOutput("SSR Reprojected", params, Stg::ComputeShader);
        }
        { // 8x8 average reflections texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 7) / 8;
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            avg_refl_tex = data->out_avg_refl_tex =
                ssr_reproject.AddStorageImageOutput("Average Refl", params, Stg::ComputeShader);
        }
        { // Variance
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            variance_temp_tex = data->out_variance_tex =
                ssr_reproject.AddStorageImageOutput("Variance Temp", params, Stg::ComputeShader);
        }
        { // Sample count
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sample_count_tex = data->out_sample_count_tex =
                ssr_reproject.AddStorageImageOutput("Sample Count", params, Stg::ComputeShader);
        }

        data->sample_count_hist_tex =
            ssr_reproject.AddHistoryTextureInput(data->out_sample_count_tex, Stg::ComputeShader);

        ssr_reproject.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
            FgAllocTex &depth_hist_tex = builder.GetReadTexture(data->depth_hist_tex);
            FgAllocTex &norm_hist_tex = builder.GetReadTexture(data->norm_hist_tex);
            FgAllocTex &refl_hist_tex = builder.GetReadTexture(data->refl_hist_tex);
            FgAllocTex &variance_hist_tex = builder.GetReadTexture(data->variance_hist_tex);
            FgAllocTex &sample_count_hist_tex = builder.GetReadTexture(data->sample_count_hist_tex);
            FgAllocTex &relf_tex = builder.GetReadTexture(data->refl_tex);
            FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);
            FgAllocTex &out_reprojected_tex = builder.GetWriteTexture(data->out_reprojected_tex);
            FgAllocTex &out_avg_refl_tex = builder.GetWriteTexture(data->out_avg_refl_tex);
            FgAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);
            FgAllocTex &out_sample_count_tex = builder.GetWriteTexture(data->out_sample_count_tex);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *shared_data_buf.ref},
                {Trg::Tex2DSampled, SSRReproject::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, SSRReproject::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::DEPTH_HIST_TEX_SLOT, {*depth_hist_tex.ref, 1}},
                {Trg::Tex2DSampled, SSRReproject::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::REFL_HIST_TEX_SLOT, *refl_hist_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::REFL_TEX_SLOT, *relf_tex.ref},
                {Trg::Tex2DSampled, SSRReproject::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                {Trg::SBufRO, SSRReproject::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Trg::Image2D, SSRReproject::OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
                {Trg::Image2D, SSRReproject::OUT_AVG_REFL_IMG_SLOT, *out_avg_refl_tex.ref},
                {Trg::Image2D, SSRReproject::OUT_VERIANCE_IMG_SLOT, *out_variance_tex.ref},
                {Trg::Image2D, SSRReproject::OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

            SSRReproject::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_ssr_reproject_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    FgResRef prefiltered_refl, variance_temp2_tex;

    { // Denoiser prefilter
        auto &ssr_prefilter = fg_builder_.AddNode("SSR PREFILTER");

        struct PassData {
            FgResRef depth_tex, norm_tex;
            FgResRef avg_refl_tex, refl_tex;
            FgResRef variance_tex, sample_count_tex;
            FgResRef exposure_tex;
            FgResRef tile_list, indir_args;
            uint32_t indir_args_offset = 0;
            FgResRef out_refl_tex, out_variance_tex;
        };

        auto *data = ssr_prefilter.AllocNodeData<PassData>();
        data->depth_tex = ssr_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = ssr_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl_tex = ssr_prefilter.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->refl_tex = ssr_prefilter.AddTextureInput(refl_tex, Stg::ComputeShader);
        data->variance_tex = ssr_prefilter.AddTextureInput(variance_temp_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_prefilter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->exposure_tex = ssr_prefilter.AddHistoryTextureInput("Exposure", Stg::ComputeShader);
        data->tile_list = ssr_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reflection color texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            prefiltered_refl = data->out_refl_tex =
                ssr_prefilter.AddStorageImageOutput("SSR Denoised 1", params, Stg::ComputeShader);
        }
        { // Variance
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            variance_temp2_tex = data->out_variance_tex =
                ssr_prefilter.AddStorageImageOutput("Variance Temp 2", params, Stg::ComputeShader);
        }

        ssr_prefilter.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &avg_refl_tex = builder.GetReadTexture(data->avg_refl_tex);
            FgAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
            FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex &out_refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            FgAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {
                {Trg::Tex2DSampled, SSRPrefilter::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, SSRPrefilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, SSRPrefilter::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                {Trg::Tex2DSampled, SSRPrefilter::REFL_TEX_SLOT, *refl_tex.ref},
                {Trg::Tex2DSampled, SSRPrefilter::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Trg::Tex2DSampled, SSRPrefilter::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Trg::Tex2DSampled, SSRPrefilter::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                {Trg::SBufRO, SSRPrefilter::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Trg::Image2D, SSRPrefilter::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Trg::Image2D, SSRPrefilter::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            const auto grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + SSRPrefilter::LOCAL_GROUP_SIZE_X - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + SSRPrefilter::LOCAL_GROUP_SIZE_Y - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_Y,
                1u};

            SSRPrefilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_ssr_prefilter_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    FgResRef gi_specular_tex, ssr_variance_tex;

    { // Denoiser accumulation
        auto &ssr_temporal = fg_builder_.AddNode("SSR TEMPORAL");

        struct PassData {
            FgResRef shared_data;
            FgResRef norm_tex, avg_refl_tex, refl_tex, reproj_refl_tex;
            FgResRef variance_tex, sample_count_tex, tile_list;
            FgResRef exposure_tex;
            FgResRef indir_args;
            uint32_t indir_args_offset = 0;
            FgResRef out_refl_tex, out_variance_tex;
        };

        auto *data = ssr_temporal.AllocNodeData<PassData>();
        data->shared_data = ssr_temporal.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        data->norm_tex = ssr_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl_tex = ssr_temporal.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->refl_tex = ssr_temporal.AddTextureInput(prefiltered_refl, Stg::ComputeShader);
        data->reproj_refl_tex = ssr_temporal.AddTextureInput(reproj_refl_tex, Stg::ComputeShader);
        data->variance_tex = ssr_temporal.AddTextureInput(variance_temp2_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_temporal.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->exposure_tex = ssr_temporal.AddHistoryTextureInput("Exposure", Stg::ComputeShader);
        data->tile_list = ssr_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        if (EnableBlur) {
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_specular_tex = data->out_refl_tex =
                ssr_temporal.AddStorageImageOutput("GI Specular", params, Stg::ComputeShader);
        } else {
            gi_specular_tex = refl_tex = data->out_refl_tex =
                ssr_temporal.AddStorageImageOutput(refl_tex, Stg::ComputeShader);
        }
        { // Variance texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssr_variance_tex = data->out_variance_tex =
                ssr_temporal.AddStorageImageOutput("Variance", params, Stg::ComputeShader);
        }

        ssr_temporal.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &avg_refl_tex = builder.GetReadTexture(data->avg_refl_tex);
            FgAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
            FgAllocTex &reproj_refl_tex = builder.GetReadTexture(data->reproj_refl_tex);
            FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex &out_refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            FgAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::REFL_TEX_SLOT, *refl_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::REPROJ_REFL_TEX_SLOT, *reproj_refl_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Trg::Tex2DSampled, SSRResolveTemporal::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                {Trg::SBufRO, SSRResolveTemporal::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Trg::Image2D, SSRResolveTemporal::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Trg::Image2D, SSRResolveTemporal::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            SSRResolveTemporal::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_ssr_temporal_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    FgResRef refl_final = gi_specular_tex;

    if (EnableBlur) {
        FgResRef gi_specular2_tex;

        { // Denoiser blur
            auto &ssr_blur = fg_builder_.AddNode("SSR BLUR");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, gi_tex;
                FgResRef sample_count_tex, variance_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset = 0;
                FgResRef out_gi_tex;
            };

            auto *data = ssr_blur.AllocNodeData<PassData>();
            data->shared_data = ssr_blur.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
            data->depth_tex = ssr_blur.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec_tex = ssr_blur.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm_tex = ssr_blur.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi_tex = ssr_blur.AddTextureInput(gi_specular_tex, Stg::ComputeShader);
            data->sample_count_tex = ssr_blur.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->variance_tex = ssr_blur.AddTextureInput(ssr_variance_tex, Stg::ComputeShader);
            data->tile_list = ssr_blur.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = ssr_blur.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset = 3 * sizeof(uint32_t);

            { // Final reflection
                Ren::Tex2DParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RawRGBA16F;
                params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                gi_specular2_tex = data->out_gi_tex =
                    ssr_blur.AddStorageImageOutput("GI Specular 2", params, Stg::ComputeShader);
            }

            ssr_blur.set_execute_cb([this, data](FgBuilder &builder) {
                FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
                FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
                FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
                FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
                FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
                FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

                const Ren::Binding bindings[] = {
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                    {Trg::Tex2DSampled, SSRBlur::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                    {Trg::Tex2DSampled, SSRBlur::SPEC_TEX_SLOT, *spec_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::NORM_TEX_SLOT, *norm_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::REFL_TEX_SLOT, *gi_tex.ref, nearest_sampler_.get()},
                    {Trg::Tex2DSampled, SSRBlur::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::VARIANCE_TEX_SLOT, *variance_tex.ref},
                    {Trg::SBufRO, SSRBlur::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                    {Trg::Image2D, SSRBlur::OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

                SSRBlur::Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[0];
                uniform_params.img_size =
                    Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                Ren::DispatchComputeIndirect(pi_ssr_blur_[0], *indir_args_buf.ref, data->indir_args_offset, bindings,
                                             &uniform_params, sizeof(uniform_params),
                                             builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        FgResRef gi_specular3_tex;

        { // Denoiser blur 2
            auto &ssr_post_blur = fg_builder_.AddNode("SSR POST BLUR");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, gi_tex, raylen_tex;
                FgResRef sample_count_tex, variance_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset = 0;
                FgResRef out_gi_tex;
            };

            auto *data = ssr_post_blur.AllocNodeData<PassData>();
            data->shared_data = ssr_post_blur.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
            data->depth_tex = ssr_post_blur.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec_tex = ssr_post_blur.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm_tex = ssr_post_blur.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi_tex = ssr_post_blur.AddTextureInput(gi_specular2_tex, Stg::ComputeShader);
            data->sample_count_tex = ssr_post_blur.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->variance_tex = ssr_post_blur.AddTextureInput(ssr_variance_tex, Stg::ComputeShader);
            data->tile_list = ssr_post_blur.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = ssr_post_blur.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset = 3 * sizeof(uint32_t);

            gi_specular3_tex = data->out_gi_tex = ssr_post_blur.AddStorageImageOutput(refl_tex, Stg::ComputeShader);

            ssr_post_blur.set_execute_cb([this, data](FgBuilder &builder) {
                FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
                FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
                FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
                FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
                FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
                FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

                const Ren::Binding bindings[] = {
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                    {Trg::Tex2DSampled, SSRBlur::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                    {Trg::Tex2DSampled, SSRBlur::SPEC_TEX_SLOT, *spec_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::NORM_TEX_SLOT, *norm_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::REFL_TEX_SLOT, *gi_tex.ref, nearest_sampler_.get()},
                    {Trg::Tex2DSampled, SSRBlur::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                    {Trg::Tex2DSampled, SSRBlur::VARIANCE_TEX_SLOT, *variance_tex.ref},
                    {Trg::SBufRO, SSRBlur::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                    {Trg::Image2D, SSRBlur::OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

                SSRBlur::Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[1];
                uniform_params.img_size =
                    Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                Ren::DispatchComputeIndirect(pi_ssr_blur_[1], *indir_args_buf.ref, data->indir_args_offset, bindings,
                                             &uniform_params, sizeof(uniform_params),
                                             builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        FgResRef gi_specular4_tex;

        { // Denoiser stabilization
            auto &ssr_stabilization = fg_builder_.AddNode("SSR STABILIZATION");

            struct PassData {
                FgResRef depth_tex, velocity_tex, ssr_tex, ssr_hist_tex;
                FgResRef exposure_tex;
                FgResRef out_ssr_tex;
            };

            auto *data = ssr_stabilization.AllocNodeData<PassData>();
            data->depth_tex = ssr_stabilization.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->velocity_tex = ssr_stabilization.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
            data->ssr_tex = ssr_stabilization.AddTextureInput(gi_specular3_tex, Stg::ComputeShader);
            data->exposure_tex = ssr_stabilization.AddHistoryTextureInput("Exposure", Stg::ComputeShader);

            { // Final gi
                Ren::Tex2DParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RawRGBA16F;
                params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                gi_specular4_tex = data->out_ssr_tex =
                    ssr_stabilization.AddStorageImageOutput("GI Specular 4", params, Stg::ComputeShader);
            }

            data->ssr_hist_tex = ssr_stabilization.AddHistoryTextureInput(gi_specular4_tex, Stg::ComputeShader);

            ssr_stabilization.set_execute_cb([this, data](FgBuilder &builder) {
                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->ssr_tex);
                FgAllocTex &gi_hist_tex = builder.GetReadTexture(data->ssr_hist_tex);
                FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_ssr_tex);

                const Ren::Binding bindings[] = {
                    {Trg::Tex2DSampled, SSRStabilization::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                    {Trg::Tex2DSampled, SSRStabilization::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                    {Trg::Tex2DSampled, SSRStabilization::SSR_TEX_SLOT, *gi_tex.ref},
                    {Trg::Tex2DSampled, SSRStabilization::SSR_HIST_TEX_SLOT, *gi_hist_tex.ref},
                    {Trg::Tex2DSampled, SSRStabilization::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                    {Trg::Image2D, SSRStabilization::OUT_SSR_IMG_SLOT, *out_gi_tex.ref}};

                const Ren::Vec3u grp_count =
                    Ren::Vec3u{(view_state_.act_res[0] + SSRStabilization::LOCAL_GROUP_SIZE_X - 1u) /
                                   SSRStabilization::LOCAL_GROUP_SIZE_X,
                               (view_state_.act_res[1] + SSRStabilization::LOCAL_GROUP_SIZE_Y - 1u) /
                                   SSRStabilization::LOCAL_GROUP_SIZE_Y,
                               1u};

                SSRStabilization::Params uniform_params;
                uniform_params.img_size =
                    Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

                Ren::DispatchCompute(pi_ssr_stabilization_, grp_count, bindings, &uniform_params,
                                     sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
            });

            if (EnableStabilization) {
                refl_final = gi_specular4_tex;
            } else {
                refl_final = gi_specular3_tex;
            }
        }
    }

    assert(deferred_shading);

    auto &ssr_compose = fg_builder_.AddNode("SSR COMPOSE NEW");

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

    data->shared_data =
        ssr_compose.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
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

    ssr_compose.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
        FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
        FgAllocTex &albedo_tex = builder.GetReadTexture(data->albedo_tex);
        FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
        FgAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
        FgAllocTex &ltc_luts = builder.GetReadTexture(data->ltc_luts);

        FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

        Ren::RastState rast_state;
        rast_state.depth.test_enabled = false;
        rast_state.depth.write_enabled = false;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.blend.enabled = true;
        rast_state.blend.src_color = unsigned(Ren::eBlendFactor::One);
        rast_state.blend.dst_color = unsigned(Ren::eBlendFactor::One);
        rast_state.blend.src_alpha = unsigned(Ren::eBlendFactor::Zero);
        rast_state.blend.dst_alpha = unsigned(Ren::eBlendFactor::One);

        rast_state.viewport[2] = view_state_.act_res[0];
        rast_state.viewport[3] = view_state_.act_res[1];

        { // compose reflections on top of clean buffer
            const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

            // TODO: get rid of global binding slots
            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::ALBEDO_TEX_SLOT, *albedo_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::SPEC_TEX_SLOT, *spec_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::NORM_TEX_SLOT, *normal_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::REFL_TEX_SLOT, *refl_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, SSRComposeNew::LTC_LUTS_TEX_SLOT, *ltc_luts.ref}};

            SSRComposeNew::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, render_targets, {}, rast_state,
                                builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
        }
    });
}

void Eng::Renderer::AddLQSpecularPasses(const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                        const FgResRef depth_down_2x, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
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
        data->shared_data =
            ssr_trace.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
        data->normal_tex = ssr_trace.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->depth_down_2x_tex = ssr_trace.AddTextureInput(depth_down_2x, Stg::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawRGB10_A2;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssr_temp1 = data->output_tex = ssr_trace.AddColorOutput("SSR Temp 1", params);
        }

        ssr_trace.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            FgAllocTex &depth_down_2x_tex = builder.GetReadTexture(data->depth_down_2x_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.scr_res[0] / 2;
            rast_state.viewport[3] = view_state_.scr_res[1] / 2;

            { // screen space tracing
                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {
                    {Trg::Tex2DSampled, SSRTrace::DEPTH_TEX_SLOT, *depth_down_2x_tex.ref},
                    {Trg::Tex2DSampled, SSRTrace::NORM_TEX_SLOT, *normal_tex.ref},
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref}};

                SSRTrace::Params uniform_params;
                uniform_params.transform =
                    Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0] / 2), float(view_state_.act_res[1] / 2)};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_prog_, render_targets, {}, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(SSRTrace::Params), 0);
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
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawRGB10_A2;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssr_temp2 = data->output_tex = ssr_dilate.AddColorOutput("SSR Temp 2", params);
        }

        ssr_dilate.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &ssr_tex = builder.GetReadTexture(data->ssr_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.scr_res[0] / 2;
            rast_state.viewport[3] = view_state_.scr_res[1] / 2;

            { // dilate ssr buffer
                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                Ren::Program *dilate_prog = blit_ssr_dilate_prog_.get();

                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, SSRDilate::SSR_TEX_SLOT, *ssr_tex.ref}};

                SSRDilate::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, rast_state.viewport[2], rast_state.viewport[3]};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_dilate_prog_, render_targets, {}, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(SSRDilate::Params), 0);
            }
        });
    }
    { // Compose
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
    }
}