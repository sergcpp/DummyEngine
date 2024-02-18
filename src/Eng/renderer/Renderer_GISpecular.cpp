#include "Renderer.h"

#include <Ren/Context.h>

#include "shaders/blit_ssr_compose_new_interface.h"
#include "shaders/blit_ssr_dilate_interface.h"
#include "shaders/blit_ssr_interface.h"
#include "shaders/ssr_classify_tiles_interface.h"
#include "shaders/ssr_prefilter_interface.h"
#include "shaders/ssr_reproject_interface.h"
#include "shaders/ssr_resolve_temporal_interface.h"
#include "shaders/ssr_trace_hq_interface.h"
#include "shaders/ssr_write_indir_rt_dispatch_interface.h"
#include "shaders/ssr_write_indirect_args_interface.h"

void Eng::Renderer::AddHQSpecularPasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                                        const Ren::WeakTex2DRef lm_indir_sh[4], const bool deferred_shading,
                                        const bool debug_denoise, const Ren::ProbeStorage *probe_storage,
                                        const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                        const AccelerationStructureData &acc_struct_data,
                                        const BindlessTextureData &bindless, const RpResRef depth_hierarchy,
                                        RpResRef rt_obj_instances_res, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    // Reflection settings
    static const float GlossyThreshold = 1.05f; // slightly above 1 to make sure comparison is always true (for now)
    static const float MirrorThreshold = 0.0001f;
    static const int SamplesPerQuad = 1;
    static const bool VarianceGuided = true;

    RpResRef ray_counter, raylen_tex;

    { // Prepare atomic counter and ray length texture
        auto &ssr_prepare = rp_builder_.AddPass("SSR PREPARE");

        struct PassData {
            RpResRef ray_counter;
            RpResRef ray_length_tex;
        };

        auto *data = ssr_prepare.AllocPassData<PassData>();

        { // ray counter
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = 6 * sizeof(uint32_t);

            ray_counter = data->ray_counter = ssr_prepare.AddTransferOutput("Ray Counter", desc);
        }
        { // ray length
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            raylen_tex = data->ray_length_tex = ssr_prepare.AddTransferImageOutput("Refl Ray Length", params);
        }

        ssr_prepare.set_execute_cb([data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocTex &raylen_tex = builder.GetWriteTexture(data->ray_length_tex);

            Ren::Context &ctx = builder.ctx();
            ray_counter_buf.ref->Fill(0, ray_counter_buf.ref->size(), 0, ctx.current_cmd_buf());

            static const float clear_color[4] = {};
            Ren::ClearImage(*raylen_tex.ref, clear_color, ctx.current_cmd_buf());
        });
    }

    RpResRef ray_list, tile_list;
    RpResRef refl_tex, noise_tex;

    const bool hq_hdr = (render_flags_ & EnableHQ_HDR) != 0;

    { // Classify pixel quads
        auto &ssr_classify = rp_builder_.AddPass("SSR CLASSIFY");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef variance_history;
            RpResRef sobol, scrambling_tile, ranking_tile;
            RpResRef ray_counter;
            RpResRef ray_list;
            RpResRef tile_list;
            RpResRef out_refl_tex, out_noise_tex;
        };

        auto *data = ssr_classify.AllocPassData<PassData>();
        data->depth = ssr_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normal = ssr_classify.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->variance_history = ssr_classify.AddHistoryTextureInput("Variance", Stg::ComputeShader);
        data->sobol = ssr_classify.AddStorageReadonlyInput(sobol_seq_buf_, Stg::ComputeShader);
        data->scrambling_tile = ssr_classify.AddStorageReadonlyInput(scrambling_tile_1spp_buf_, Stg::ComputeShader);
        data->ranking_tile = ssr_classify.AddStorageReadonlyInput(ranking_tile_1spp_buf_, Stg::ComputeShader);
        ray_counter = data->ray_counter = ssr_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = ssr_classify.AddStorageOutput("Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 7) / 8) * sizeof(uint32_t);

            tile_list = data->tile_list = ssr_classify.AddStorageOutput("Tile List", desc, Stg::ComputeShader);
        }
        { // reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            if (hq_hdr) {
                params.format = Ren::eTexFormat::RawRGBA16F;
            } else {
                params.format = Ren::eTexFormat::RawRG11F_B10F;
            }
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            refl_tex = data->out_refl_tex =
                ssr_classify.AddStorageImageOutput("SSR Temp 2", params, Stg::ComputeShader);
        }
        { // blue noise texture
            Ren::Tex2DParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RawRG88;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex =
                ssr_classify.AddStorageImageOutput("Blue Noise Tex", params, Stg::ComputeShader);
        }

        ssr_classify.set_execute_cb([this, data, hq_hdr](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_history);
            RpAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &ray_list_buf = builder.GetWriteBuffer(data->ray_list);
            RpAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            RpAllocTex &refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            RpAllocTex &noise_tex = builder.GetWriteTexture(data->out_noise_tex);

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
                {Trg::Tex2D, SSRClassifyTiles::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Trg::Tex2D, SSRClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2D, SSRClassifyTiles::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Trg::SBuf, SSRClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Trg::SBuf, SSRClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
                {Trg::SBuf, SSRClassifyTiles::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Trg::TBuf, SSRClassifyTiles::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Trg::TBuf, SSRClassifyTiles::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Trg::TBuf, SSRClassifyTiles::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Trg::Image, SSRClassifyTiles::REFL_IMG_SLOT, *refl_tex.ref},
                {Trg::Image, SSRClassifyTiles::NOISE_IMG_SLOT, *noise_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + SSRClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) /
                               SSRClassifyTiles::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + SSRClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) /
                               SSRClassifyTiles::LOCAL_GROUP_SIZE_Y,
                           1u};

            SSRClassifyTiles::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, MirrorThreshold};
            uniform_params.samples_and_guided = Ren::Vec2u{uint32_t(SamplesPerQuad), VarianceGuided ? 1u : 0u};
            uniform_params.frame_index = view_state_.frame_index;

            Ren::DispatchCompute(pi_ssr_classify_tiles_[hq_hdr], grp_count, bindings, &uniform_params,
                                 sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef indir_disp_buf;

    { // Write indirect arguments
        auto &write_indir = rp_builder_.AddPass("SSR INDIR ARGS");

        struct PassData {
            RpResRef ray_counter;
            RpResRef indir_disp_buf;
        };

        auto *data = write_indir.AllocPassData<PassData>();
        ray_counter = data->ray_counter = write_indir.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // Indirect arguments
            RpBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp_buf = data->indir_disp_buf =
                write_indir.AddStorageOutput("Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {{Trg::SBuf, SSRWriteIndirectArgs::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                             {Trg::SBuf, SSRWriteIndirectArgs::INDIR_ARGS_SLOT, *indir_args.ref}};

            Ren::DispatchCompute(pi_ssr_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                                 builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef ray_rt_list;

    { // Trace rays
        auto &ssr_trace_hq = rp_builder_.AddPass("SSR TRACE HQ");

        struct PassData {
            RpResRef noise_tex;
            RpResRef shared_data;
            RpResRef color_tex, normal_tex, depth_hierarchy;

            RpResRef in_ray_list, indir_args, inout_ray_counter;
            RpResRef refl_tex, raylen_tex, out_ray_list;
        };

        auto *data = ssr_trace_hq.AllocPassData<PassData>();
        data->noise_tex = ssr_trace_hq.AddTextureInput(noise_tex, Stg::ComputeShader);
        data->shared_data = ssr_trace_hq.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        data->color_tex = ssr_trace_hq.AddTextureInput(frame_textures.color, Stg::ComputeShader);
        data->normal_tex = ssr_trace_hq.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->depth_hierarchy = ssr_trace_hq.AddTextureInput(depth_hierarchy, Stg::ComputeShader);

        data->in_ray_list = ssr_trace_hq.AddStorageReadonlyInput(ray_list, Stg::ComputeShader);
        data->indir_args = ssr_trace_hq.AddIndirectBufferInput(indir_disp_buf);
        ray_counter = data->inout_ray_counter = ssr_trace_hq.AddStorageOutput(ray_counter, Stg::ComputeShader);
        refl_tex = data->refl_tex = ssr_trace_hq.AddStorageImageOutput(refl_tex, Stg::ComputeShader);
        raylen_tex = data->raylen_tex = ssr_trace_hq.AddStorageImageOutput(raylen_tex, Stg::ComputeShader);

        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list = ssr_trace_hq.AddStorageOutput("Ray RT List", desc, Stg::ComputeShader);
        }

        ssr_trace_hq.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &noise_tex = builder.GetReadTexture(data->noise_tex);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &color_tex = builder.GetReadTexture(data->color_tex);
            RpAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            RpAllocTex &depth_hierarchy_tex = builder.GetReadTexture(data->depth_hierarchy);
            RpAllocBuf &in_ray_list_buf = builder.GetReadBuffer(data->in_ray_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_refl_tex = builder.GetWriteTexture(data->refl_tex);
            RpAllocTex &out_raylen_tex = builder.GetWriteTexture(data->raylen_tex);
            RpAllocBuf &inout_ray_counter_buf = builder.GetWriteBuffer(data->inout_ray_counter);
            RpAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(data->out_ray_list);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2D, SSRTraceHQ::DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                {Trg::Tex2D, SSRTraceHQ::COLOR_TEX_SLOT, *color_tex.ref},
                {Trg::Tex2D, SSRTraceHQ::NORM_TEX_SLOT, *normal_tex.ref},
                {Trg::Tex2D, SSRTraceHQ::NOISE_TEX_SLOT, *noise_tex.ref},
                {Trg::SBuf, SSRTraceHQ::IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                {Trg::Image, SSRTraceHQ::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Trg::Image, SSRTraceHQ::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref},
                {Trg::SBuf, SSRTraceHQ::INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                {Trg::SBuf, SSRTraceHQ::OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};

            SSRTraceHQ::Params uniform_params;
            uniform_params.resolution =
                Ren::Vec4u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1]), 0, 0};

            Ren::DispatchComputeIndirect(pi_ssr_trace_hq_, *indir_args_buf.ref, 0, bindings, &uniform_params,
                                         sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    if ((ctx_.capabilities.raytracing || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf && env_map) {
        RpResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = rp_builder_.AddPass("RT DISPATCH ARGS");

            struct PassData {
                RpResRef ray_counter;
                RpResRef indir_disp_buf;
            };

            auto *data = rt_disp_args.AllocPassData<PassData>();
            ray_counter = data->ray_counter = rt_disp_args.AddStorageOutput(ray_counter, Stg::ComputeShader);

            { // Indirect arguments
                RpBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp_buf = data->indir_disp_buf =
                    rt_disp_args.AddStorageOutput("RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](RpBuilder &builder) {
                RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                RpAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {
                    {Trg::SBuf, SSRWriteIndirRTDispatch::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                    {Trg::SBuf, SSRWriteIndirRTDispatch::INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                Ren::DispatchCompute(pi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                                     builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace reflection rays
            auto &rt_refl = rp_builder_.AddPass("RT REFLECTIONS");

            auto *data = rt_refl.AllocPassData<RpRTReflectionsData>();

            const auto stage = ctx_.capabilities.ray_query ? Stg::ComputeShader : Stg::RayTracingShader;

            data->geo_data = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stage);
            data->materials = rt_refl.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
            data->vtx_buf2 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
            data->ndx_buf = rt_refl.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
            data->shared_data = rt_refl.AddUniformBufferInput(common_buffers.shared_data_res, stage);
            data->noise_tex = rt_refl.AddTextureInput(noise_tex, stage);
            data->depth_tex = rt_refl.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_refl.AddTextureInput(frame_textures.normal, stage);
            data->env_tex = rt_refl.AddTextureInput(env_map, stage);
            data->ray_counter = rt_refl.AddStorageReadonlyInput(ray_counter, stage);
            data->ray_list = rt_refl.AddStorageReadonlyInput(ray_rt_list, stage);
            data->indir_args = rt_refl.AddIndirectBufferInput(indir_rt_disp_buf);
            data->tlas_buf = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
            data->lights_buf = rt_refl.AddStorageReadonlyInput(common_buffers.lights_res, stage);

            data->shadowmap_tex = rt_refl.AddTextureInput(shadow_map_tex_, stage);
            data->ltc_luts_tex = rt_refl.AddTextureInput(ltc_luts_, stage);

            if (!ctx_.capabilities.raytracing) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = rt_refl.AddStorageReadonlyInput(persistent_data.rt_blas_buf, stage);
                data->swrt.prim_ndx_buf =
                    rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
                data->swrt.meshes_buf = rt_refl.AddStorageReadonlyInput(persistent_data.swrt.rt_meshes_buf, stage);
                data->swrt.mesh_instances_buf = rt_refl.AddStorageReadonlyInput(rt_obj_instances_res, stage);

#if defined(USE_GL_RENDER)
                data->swrt.textures_buf = rt_refl.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
            }

            if (lm_direct) {
                data->lm_tex[0] = rt_refl.AddTextureInput(lm_direct, stage);
            }

            for (int i = 0; i < 4; ++i) {
                if (lm_indir_sh[i]) {
                    data->lm_tex[i + 1] = rt_refl.AddTextureInput(lm_indir_sh[i], stage);
                }
            }

            data->dummy_black = rt_refl.AddTextureInput(dummy_black_, stage);

            data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

            refl_tex = data->out_refl_tex = rt_refl.AddStorageImageOutput(refl_tex, stage);
            raylen_tex = data->out_raylen_tex = rt_refl.AddStorageImageOutput(raylen_tex, stage);

            rp_rt_reflections_.Setup(rp_builder_, &view_state_, &bindless, data);
            rt_refl.set_executor(&rp_rt_reflections_);
        }
    }

    RpResRef reproj_refl_tex, avg_refl_tex;
    RpResRef depth_hist_tex, sample_count_tex, variance_temp_tex;

    { // Denoiser reprojection
        auto &ssr_reproject = rp_builder_.AddPass("SSR REPROJECT");

        struct PassData {
            RpResRef shared_data;
            RpResRef depth_tex, norm_tex, velocity_tex;
            RpResRef depth_hist_tex, norm_hist_tex, refl_hist_tex, variance_hist_tex, sample_count_hist_tex;
            RpResRef refl_tex, raylen_tex;
            RpResRef tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_reprojected_tex, out_avg_refl_tex;
            RpResRef out_variance_tex, out_sample_count_tex;
        };

        auto *data = ssr_reproject.AllocPassData<PassData>();
        data->shared_data = ssr_reproject.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        data->depth_tex = ssr_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = ssr_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity_tex = ssr_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist_tex = ssr_reproject.AddHistoryTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_hist_tex = ssr_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->refl_hist_tex = ssr_reproject.AddHistoryTextureInput("GI Specular", Stg::ComputeShader);
        data->variance_hist_tex = ssr_reproject.AddHistoryTextureInput("Variance", Stg::ComputeShader);
        data->refl_tex = ssr_reproject.AddTextureInput(refl_tex, Stg::ComputeShader);
        data->raylen_tex = ssr_reproject.AddTextureInput(raylen_tex, Stg::ComputeShader);

        data->tile_list = ssr_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reprojected reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            if (hq_hdr) {
                params.format = Ren::eTexFormat::RawRGBA16F;
            } else {
                params.format = Ren::eTexFormat::RawRG11F_B10F;
            }
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            reproj_refl_tex = data->out_reprojected_tex =
                ssr_reproject.AddStorageImageOutput("SSR Reprojected", params, Stg::ComputeShader);
        }
        { // 8x8 average reflections texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 7) / 8;
            if (hq_hdr) {
                params.format = Ren::eTexFormat::RawRGBA16F;
            } else {
                params.format = Ren::eTexFormat::RawRG11F_B10F;
            }
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

        ssr_reproject.set_execute_cb([this, data, hq_hdr](RpBuilder &builder) {
            RpAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
            RpAllocTex &depth_hist_tex = builder.GetReadTexture(data->depth_hist_tex);
            RpAllocTex &norm_hist_tex = builder.GetReadTexture(data->norm_hist_tex);
            RpAllocTex &refl_hist_tex = builder.GetReadTexture(data->refl_hist_tex);
            RpAllocTex &variance_hist_tex = builder.GetReadTexture(data->variance_hist_tex);
            RpAllocTex &sample_count_hist_tex = builder.GetReadTexture(data->sample_count_hist_tex);
            RpAllocTex &relf_tex = builder.GetReadTexture(data->refl_tex);
            RpAllocTex &raylen_tex = builder.GetReadTexture(data->raylen_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);
            RpAllocTex &out_reprojected_tex = builder.GetWriteTexture(data->out_reprojected_tex);
            RpAllocTex &out_avg_refl_tex = builder.GetWriteTexture(data->out_avg_refl_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);
            RpAllocTex &out_sample_count_tex = builder.GetWriteTexture(data->out_sample_count_tex);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *shared_data_buf.ref},
                {Trg::Tex2D, SSRReproject::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Trg::Tex2D, SSRReproject::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2D, SSRReproject::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Trg::Tex2D, SSRReproject::DEPTH_HIST_TEX_SLOT, *depth_hist_tex.ref},
                {Trg::Tex2D, SSRReproject::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
                {Trg::Tex2D, SSRReproject::REFL_HIST_TEX_SLOT, *refl_hist_tex.ref},
                {Trg::Tex2D, SSRReproject::VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
                {Trg::Tex2D, SSRReproject::SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
                {Trg::Tex2D, SSRReproject::REFL_TEX_SLOT, *relf_tex.ref},
                {Trg::Tex2D, SSRReproject::RAYLEN_TEX_SLOT, *raylen_tex.ref},
                {Trg::SBuf, SSRReproject::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Trg::Image, SSRReproject::OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
                {Trg::Image, SSRReproject::OUT_AVG_REFL_IMG_SLOT, *out_avg_refl_tex.ref},
                {Trg::Image, SSRReproject::OUT_VERIANCE_IMG_SLOT, *out_variance_tex.ref},
                {Trg::Image, SSRReproject::OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

            SSRReproject::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, 0.0f};

            Ren::DispatchComputeIndirect(pi_ssr_reproject_[hq_hdr], *indir_args_buf.ref, data->indir_args_offset,
                                         bindings, &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef prefiltered_refl, variance_temp2_tex;

    { // Denoiser prefilter
        auto &ssr_prefilter = rp_builder_.AddPass("SSR PREFILTER");

        struct PassData {
            RpResRef depth_tex, norm_tex;
            RpResRef avg_refl_tex, refl_tex;
            RpResRef variance_tex, sample_count_tex;
            RpResRef tile_list, indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_refl_tex, out_variance_tex;
        };

        auto *data = ssr_prefilter.AllocPassData<PassData>();
        data->depth_tex = ssr_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = ssr_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl_tex = ssr_prefilter.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->refl_tex = ssr_prefilter.AddTextureInput(refl_tex, Stg::ComputeShader);
        data->variance_tex = ssr_prefilter.AddTextureInput(variance_temp_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_prefilter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->tile_list = ssr_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reflection color texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            if (hq_hdr) {
                params.format = Ren::eTexFormat::RawRGBA16F;
            } else {
                params.format = Ren::eTexFormat::RawRG11F_B10F;
            }
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

        ssr_prefilter.set_execute_cb([this, data, hq_hdr](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &avg_refl_tex = builder.GetReadTexture(data->avg_refl_tex);
            RpAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2D, SSRPrefilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                                             {Trg::Tex2D, SSRPrefilter::NORM_TEX_SLOT, *norm_tex.ref},
                                             {Trg::Tex2D, SSRPrefilter::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                                             {Trg::Tex2D, SSRPrefilter::REFL_TEX_SLOT, *refl_tex.ref},
                                             {Trg::Tex2D, SSRPrefilter::VARIANCE_TEX_SLOT, *variance_tex.ref},
                                             {Trg::Tex2D, SSRPrefilter::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                                             {Trg::SBuf, SSRPrefilter::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},

                                             {Trg::Image, SSRPrefilter::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                                             {Trg::Image, SSRPrefilter::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            const auto grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + SSRPrefilter::LOCAL_GROUP_SIZE_X - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + SSRPrefilter::LOCAL_GROUP_SIZE_Y - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_Y,
                1u};

            SSRPrefilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, MirrorThreshold};

            Ren::DispatchComputeIndirect(pi_ssr_prefilter_[hq_hdr], *indir_args_buf.ref, data->indir_args_offset,
                                         bindings, &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef gi_specular_tex;

    { // Denoiser accumulation
        auto &ssr_temporal = rp_builder_.AddPass("SSR TEMPORAL");

        struct PassData {
            RpResRef shared_data;
            RpResRef norm_tex, avg_refl_tex, refl_tex, reproj_refl_tex;
            RpResRef variance_tex, sample_count_tex, tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_refl_tex, out_variance_tex;
        };

        auto *data = ssr_temporal.AllocPassData<PassData>();
        data->shared_data = ssr_temporal.AddUniformBufferInput(common_buffers.shared_data_res, Stg::ComputeShader);
        data->norm_tex = ssr_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_refl_tex = ssr_temporal.AddTextureInput(avg_refl_tex, Stg::ComputeShader);
        data->refl_tex = ssr_temporal.AddTextureInput(prefiltered_refl, Stg::ComputeShader);
        data->reproj_refl_tex = ssr_temporal.AddTextureInput(reproj_refl_tex, Stg::ComputeShader);
        data->variance_tex = ssr_temporal.AddTextureInput(variance_temp2_tex, Stg::ComputeShader);
        data->sample_count_tex = ssr_temporal.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->tile_list = ssr_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = ssr_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Final reflection
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            if (hq_hdr) {
                params.format = Ren::eTexFormat::RawRGBA16F;
            } else {
                params.format = Ren::eTexFormat::RawRG11F_B10F;
            }
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_specular_tex = data->out_refl_tex =
                ssr_temporal.AddStorageImageOutput("GI Specular", params, Stg::ComputeShader);
        }
        { // Variance texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            data->out_variance_tex = ssr_temporal.AddStorageImageOutput("Variance", params, Stg::ComputeShader);
        }

        ssr_temporal.set_execute_cb([this, data, hq_hdr](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &avg_refl_tex = builder.GetReadTexture(data->avg_refl_tex);
            RpAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
            RpAllocTex &reproj_refl_tex = builder.GetReadTexture(data->reproj_refl_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_refl_tex = builder.GetWriteTexture(data->out_refl_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2D, SSRResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
                {Trg::Tex2D, SSRResolveTemporal::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                {Trg::Tex2D, SSRResolveTemporal::REFL_TEX_SLOT, *refl_tex.ref},
                {Trg::Tex2D, SSRResolveTemporal::REPROJ_REFL_TEX_SLOT, *reproj_refl_tex.ref},
                {Trg::Tex2D, SSRResolveTemporal::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Trg::Tex2D, SSRResolveTemporal::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Trg::SBuf, SSRResolveTemporal::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Trg::Image, SSRResolveTemporal::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Trg::Image, SSRResolveTemporal::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            SSRResolveTemporal::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, MirrorThreshold};

            Ren::DispatchComputeIndirect(pi_ssr_resolve_temporal_[hq_hdr], *indir_args_buf.ref, data->indir_args_offset,
                                         bindings, &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    if (!deferred_shading) {
        auto &ssr_compose = rp_builder_.AddPass("SSR COMPOSE");

        auto *data = ssr_compose.AllocPassData<RpSSRCompose2Data>();

        data->shared_data =
            ssr_compose.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
        data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);

        if (debug_denoise) {
            data->refl_tex = ssr_compose.AddTextureInput(refl_tex, Stg::FragmentShader);
        } else {
            data->refl_tex = ssr_compose.AddTextureInput(gi_specular_tex, Stg::FragmentShader);
        }
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Stg::FragmentShader);
        frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

        rp_ssr_compose2_.Setup(&view_state_, probe_storage, data);
        ssr_compose.set_executor(&rp_ssr_compose2_);
    } else {
        auto &ssr_compose = rp_builder_.AddPass("SSR COMPOSE NEW");

        struct PassData {
            RpResRef shared_data;
            RpResRef depth_tex;
            RpResRef normal_tex;
            RpResRef albedo_tex;
            RpResRef spec_tex;
            RpResRef refl_tex;
            RpResRef brdf_lut;

            RpResRef output_tex;
        };

        auto *data = ssr_compose.AllocPassData<PassData>();

        data->shared_data =
            ssr_compose.AddUniformBufferInput(common_buffers.shared_data_res, Stg::VertexShader | Stg::FragmentShader);
        data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
        data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Stg::FragmentShader);
        data->albedo_tex = ssr_compose.AddTextureInput(frame_textures.albedo, Stg::FragmentShader);
        data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Stg::FragmentShader);

        if (debug_denoise) {
            data->refl_tex = ssr_compose.AddTextureInput(refl_tex, Stg::FragmentShader);
        } else {
            data->refl_tex = ssr_compose.AddTextureInput(gi_specular_tex, Stg::FragmentShader);
        }
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Stg::FragmentShader);
        frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

        ssr_compose.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            RpAllocTex &albedo_tex = builder.GetReadTexture(data->albedo_tex);
            RpAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
            RpAllocTex &refl_tex = builder.GetReadTexture(data->refl_tex);
            RpAllocTex &brdf_lut = builder.GetReadTexture(data->brdf_lut);

            RpAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.depth.test_enabled = false;
            rast_state.depth.write_enabled = false;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.blend.enabled = true;
            rast_state.blend.src = unsigned(Ren::eBlendFactor::One);
            rast_state.blend.dst = unsigned(Ren::eBlendFactor::One);

            rast_state.viewport[2] = view_state_.act_res[0];
            rast_state.viewport[3] = view_state_.act_res[1];

            { // compose reflections on top of clean buffer
                const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};

                // TODO: get rid of global binding slots
                const Ren::Binding bindings[] = {
                    {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock),
                     *unif_sh_data_buf.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::ALBEDO_TEX_SLOT, *albedo_tex.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::SPEC_TEX_SLOT, *spec_tex.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::DEPTH_TEX_SLOT, *depth_tex.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::NORM_TEX_SLOT, *normal_tex.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::REFL_TEX_SLOT, *refl_tex.ref},
                    {Ren::eBindTarget::Tex2D, SSRComposeNew::BRDF_TEX_SLOT, *brdf_lut.ref},
                };

                SSRComposeNew::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_compose_prog_, render_targets, {}, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(uniform_params), 0);
            }
        });
    }
}

void Eng::Renderer::AddLQSpecularPasses(const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                        const RpResRef depth_down_2x, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    RpResRef ssr_temp1;
    { // Trace
        auto &ssr_trace = rp_builder_.AddPass("SSR TRACE");

        struct PassData {
            RpResRef shared_data;
            RpResRef normal_tex;
            RpResRef depth_down_2x_tex;
            RpResRef output_tex;
        };

        auto *data = ssr_trace.AllocPassData<PassData>();
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

        ssr_trace.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            RpAllocTex &depth_down_2x_tex = builder.GetReadTexture(data->depth_down_2x_tex);
            RpAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

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
                    {Trg::Tex2D, SSRTrace::DEPTH_TEX_SLOT, *depth_down_2x_tex.ref},
                    {Trg::Tex2D, SSRTrace::NORM_TEX_SLOT, *normal_tex.ref},
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_sh_data_buf.ref}};

                SSRTrace::Params uniform_params;
                uniform_params.transform =
                    Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0] / 2), float(view_state_.act_res[1] / 2)};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_prog_, render_targets, {}, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(SSRTrace::Params), 0);
            }
        });
    }
    RpResRef ssr_temp2;
    { // Dilate
        auto &ssr_dilate = rp_builder_.AddPass("SSR DILATE");

        struct PassData {
            RpResRef ssr_tex;
            RpResRef output_tex;
        };

        auto *data = ssr_dilate.AllocPassData<PassData>();
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

        ssr_dilate.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &ssr_tex = builder.GetReadTexture(data->ssr_tex);
            RpAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

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

                const Ren::Binding bindings[] = {{Trg::Tex2D, SSRDilate::SSR_TEX_SLOT, *ssr_tex.ref}};

                SSRDilate::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, rast_state.viewport[2], rast_state.viewport[3]};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ssr_dilate_prog_, render_targets, {}, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(SSRDilate::Params), 0);
            }
        });
    }
    { // Compose
        auto &ssr_compose = rp_builder_.AddPass("SSR COMPOSE");

        auto *data = ssr_compose.AllocPassData<RpSSRComposeData>();
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

        rp_ssr_compose_.Setup(rp_builder_, &view_state_, probe_storage, data);
        ssr_compose.set_executor(&rp_ssr_compose_);
    }
}