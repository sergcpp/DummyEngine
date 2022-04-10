#include "Renderer.h"

#include "../assets/shaders/internal/ssr_classify_tiles_interface.glsl"

void Renderer::AddHQSpecularPasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                                   const Ren::WeakTex2DRef lm_indir_sh[4], const bool debug_denoise,
                                   const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                   const PersistentGpuData &persistent_data,
                                   const AccelerationStructureData &acc_struct_data,
                                   const BindlessTextureData &bindless, const RpResRef depth_hierarchy,
                                   FrameTextures &frame_textures) {
    // Reflection settings
    static const float GlossyThreshold = 1.05f; // slightly above 1 to make sure comparison is always true (for now)
    static const float MirrorThreshold = 0.0001f;
    static const int SamplesPerQuad = 1;
    static const bool VarianceGuided = true;

    RpResRef ray_counter;

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
        {
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            data->ray_length_tex = ssr_prepare.AddTransferImageOutput("Refl Ray Length", params);
        }

        ssr_prepare.set_execute_cb([data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocTex &raylen_tex = builder.GetWriteTexture(data->ray_length_tex);

            Ren::Context &ctx = builder.ctx();
            Ren::FillBuffer(*ray_counter_buf.ref, 0, ray_counter_buf.ref->size(), 0, ctx.current_cmd_buf());

            float clear_color[4] = {};
            Ren::ClearColorImage(*raylen_tex.ref, clear_color, ctx.current_cmd_buf());
        });
    }

    RpResRef ray_list, tile_list;
    RpResRef refl_tex;

    { // Classify pixel quads
        auto &ssr_classify = rp_builder_.AddPass("SSR CLASSIFY");

        struct PassData {
            RpResRef depth;
            RpResRef normal;
            RpResRef variance_history;
            RpResRef ray_counter;
            RpResRef ray_list;
            RpResRef tile_list;
            RpResRef out_refl_tex;
        };

        auto *data = ssr_classify.AllocPassData<PassData>();
        data->depth = ssr_classify.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->normal = ssr_classify.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->variance_history = ssr_classify.AddTextureInput(variance_tex_[0], Ren::eStageBits::ComputeShader);
        ray_counter = data->ray_counter = ssr_classify.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);

        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = ssr_classify.AddStorageOutput("Ray List", desc, Ren::eStageBits::ComputeShader);
        }
        { // tile list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 7) / 8) * sizeof(uint32_t);

            tile_list = data->tile_list =
                ssr_classify.AddStorageOutput("Tile List", desc, Ren::eStageBits::ComputeShader);
        }
        { // reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            refl_tex = data->out_refl_tex =
                ssr_classify.AddStorageImageOutput("SSR Temp 2", params, Ren::eStageBits::ComputeShader);
        }

        ssr_classify.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->normal);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_history);

            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &ray_list_buf = builder.GetWriteBuffer(data->ray_list);
            RpAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            RpAllocTex &refl_tex = builder.GetWriteTexture(data->out_refl_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, SSRClassifyTiles::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRClassifyTiles::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRClassifyTiles::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, SSRClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
                {Ren::eBindTarget::SBuf, SSRClassifyTiles::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, SSRClassifyTiles::REFL_IMG_SLOT, *refl_tex.ref}};

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

            Ren::DispatchCompute(pi_ssr_classify_tiles_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
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
        ray_counter = data->ray_counter = write_indir.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);

        { // Indirect arguments
            RpBufDesc desc = {};
            desc.type = Ren::eBufType::Indirect;
            desc.size = 2 * sizeof(Ren::DispatchIndirectCommand);

            indir_disp_buf = data->indir_disp_buf =
                write_indir.AddStorageOutput("Intersect Args", desc, Ren::eStageBits::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::SBuf, SSRWriteIndirectArgs::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, SSRWriteIndirectArgs::INDIR_ARGS_SLOT, *indir_args.ref}};

            Ren::DispatchCompute(pi_ssr_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, COUNT_OF(bindings), nullptr,
                                 0, builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef raylen_tex, ray_rt_list;

    { // Trace rays
        auto &ssr_trace_hq = rp_builder_.AddPass("SSR TRACE HQ");

        struct PassData {
            RpResRef sobol, scrambling_tile, ranking_tile;
            RpResRef shared_data;
            RpResRef color_tex, normal_tex, depth_hierarchy;

            RpResRef in_ray_list, indir_args, inout_ray_counter;
            RpResRef refl_tex, raylen_tex, out_ray_list;
        };

        auto *data = ssr_trace_hq.AllocPassData<PassData>();
        data->sobol = ssr_trace_hq.AddStorageReadonlyInput(sobol_seq_buf_, Ren::eStageBits::ComputeShader);
        data->scrambling_tile =
            ssr_trace_hq.AddStorageReadonlyInput(scrambling_tile_1spp_buf_, Ren::eStageBits::ComputeShader);
        data->ranking_tile =
            ssr_trace_hq.AddStorageReadonlyInput(ranking_tile_1spp_buf_, Ren::eStageBits::ComputeShader);

        data->shared_data =
            ssr_trace_hq.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->color_tex = ssr_trace_hq.AddTextureInput(frame_textures.color, Ren::eStageBits::ComputeShader);
        data->normal_tex = ssr_trace_hq.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->depth_hierarchy = ssr_trace_hq.AddTextureInput(depth_hierarchy, Ren::eStageBits::ComputeShader);

        data->in_ray_list = ssr_trace_hq.AddStorageReadonlyInput(ray_list, Ren::eStageBits::ComputeShader);
        data->indir_args = ssr_trace_hq.AddIndirectBufferInput(indir_disp_buf);
        ray_counter = data->inout_ray_counter =
            ssr_trace_hq.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);
        refl_tex = data->refl_tex = ssr_trace_hq.AddStorageImageOutput(refl_tex, Ren::eStageBits::ComputeShader);

        { // Ray length texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.usage = (Ren::eTexUsage::Transfer | Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            raylen_tex = data->raylen_tex =
                ssr_trace_hq.AddStorageImageOutput("Refl Ray Length", params, Ren::eStageBits::ComputeShader);
        }
        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list =
                ssr_trace_hq.AddStorageOutput("Ray RT List", desc, Ren::eStageBits::ComputeShader);
        }

        ssr_trace_hq.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);
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
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, SSRTraceHQ::DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRTraceHQ::COLOR_TEX_SLOT, *color_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRTraceHQ::NORM_TEX_SLOT, *normal_tex.ref},
                {Ren::eBindTarget::SBuf, SSRTraceHQ::IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                {Ren::eBindTarget::TBuf, SSRTraceHQ::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, SSRTraceHQ::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, SSRTraceHQ::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Ren::eBindTarget::Image, SSRTraceHQ::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Ren::eBindTarget::Image, SSRTraceHQ::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref},
                {Ren::eBindTarget::SBuf, SSRTraceHQ::INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, SSRTraceHQ::OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};

            SSRTraceHQ::Params uniform_params;
            uniform_params.resolution =
                Ren::Vec4u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1]), 0, 0};

            Ren::DispatchComputeIndirect(pi_ssr_trace_hq_, *indir_args_buf.ref, 0, bindings, COUNT_OF(bindings),
                                         &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    if (ctx_.capabilities.raytracing && env_map) {
        RpResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = rp_builder_.AddPass("RT DISPATCH ARGS");

            struct PassData {
                RpResRef ray_counter;
                RpResRef indir_disp_buf;
            };

            auto *data = rt_disp_args.AllocPassData<PassData>();
            ray_counter = data->ray_counter =
                rt_disp_args.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);

            { // Indirect arguments
                RpBufDesc desc = {};
                desc.type = Ren::eBufType::Indirect;
                desc.size = sizeof(Ren::TraceRaysIndirectCommand) + sizeof(Ren::DispatchIndirectCommand);

                indir_rt_disp_buf = data->indir_disp_buf =
                    rt_disp_args.AddStorageOutput("RT Dispatch Args", desc, Ren::eStageBits::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](RpBuilder &builder) {
                RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                RpAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {
                    {Ren::eBindTarget::SBuf, SSRWriteIndirRTDispatch::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                    {Ren::eBindTarget::SBuf, SSRWriteIndirRTDispatch::INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                Ren::DispatchCompute(pi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, COUNT_OF(bindings),
                                     nullptr, 0, builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace reflection rays
            auto &rt_refl = rp_builder_.AddPass("RT REFLECTIONS");

            auto *data = rt_refl.AllocPassData<RpRTReflectionsData>();

            const Ren::eStageBits stage =
                ctx_.capabilities.ray_query ? Ren::eStageBits::ComputeShader : Ren::eStageBits::RayTracingShader;

            data->sobol = rt_refl.AddStorageReadonlyInput(sobol_seq_buf_, stage);
            data->scrambling_tile = rt_refl.AddStorageReadonlyInput(scrambling_tile_1spp_buf_, stage);
            data->ranking_tile = rt_refl.AddStorageReadonlyInput(ranking_tile_1spp_buf_, stage);
            data->geo_data = rt_refl.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stage);
            data->materials = rt_refl.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
            data->vtx_buf2 = rt_refl.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
            data->ndx_buf = rt_refl.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
            data->shared_data = rt_refl.AddUniformBufferInput(common_buffers.shared_data_res, stage);
            data->depth_tex = rt_refl.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_refl.AddTextureInput(frame_textures.normal, stage);
            data->env_tex = rt_refl.AddTextureInput(env_map, stage);
            data->ray_counter = rt_refl.AddStorageReadonlyInput(ray_counter, stage);
            data->ray_list = rt_refl.AddStorageReadonlyInput(ray_rt_list, stage);
            data->indir_args = rt_refl.AddIndirectBufferInput(indir_rt_disp_buf);

            if (lm_direct) {
                data->lm_tex[0] = rt_refl.AddTextureInput(lm_direct, stage);
            }

            for (int i = 0; i < 4; ++i) {
                if (lm_indir_sh[i]) {
                    data->lm_tex[i + 1] = rt_refl.AddTextureInput(lm_indir_sh[i], stage);
                }
            }

            data->dummy_black = rt_refl.AddTextureInput(dummy_black_, stage);

            raylen_tex = data->out_raylen_tex = rt_refl.AddStorageImageOutput(raylen_tex, stage);
            refl_tex = data->out_refl_tex = rt_refl.AddStorageImageOutput(refl_tex, stage);

            rp_rt_reflections_.Setup(rp_builder_, &view_state_, &acc_struct_data, &bindless, data);
            rt_refl.set_executor(&rp_rt_reflections_);
        }
    }

    RpResRef reproj_refl_tex, avg_refl_tex;
    RpResRef depth_hist_tex;

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
        data->shared_data =
            ssr_reproject.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->depth_tex = ssr_reproject.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = ssr_reproject.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->velocity_tex = ssr_reproject.AddTextureInput(frame_textures.velocity, Ren::eStageBits::ComputeShader);
        data->depth_hist_tex = ssr_reproject.AddTextureInput(depth_history_tex_, Ren::eStageBits::ComputeShader);
        data->norm_hist_tex = ssr_reproject.AddTextureInput(norm_history_tex_, Ren::eStageBits::ComputeShader);
        data->refl_hist_tex = ssr_reproject.AddTextureInput(refl_history_tex_, Ren::eStageBits::ComputeShader);
        data->variance_hist_tex = ssr_reproject.AddTextureInput(variance_tex_[0], Ren::eStageBits::ComputeShader);
        data->sample_count_hist_tex =
            ssr_reproject.AddTextureInput(sample_count_tex_[1], Ren::eStageBits::ComputeShader);
        data->refl_tex = ssr_reproject.AddTextureInput(refl_tex, Ren::eStageBits::ComputeShader);
        data->raylen_tex = ssr_reproject.AddTextureInput(raylen_tex, Ren::eStageBits::ComputeShader);

        data->tile_list = ssr_reproject.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = ssr_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reprojected reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            reproj_refl_tex = data->out_reprojected_tex =
                ssr_reproject.AddStorageImageOutput("SSR Reprojected", params, Ren::eStageBits::ComputeShader);
        }
        { // 8x8 average reflections texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 7) / 8;
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            avg_refl_tex = data->out_avg_refl_tex =
                ssr_reproject.AddStorageImageOutput("Average Refl", params, Ren::eStageBits::ComputeShader);
        }

        data->out_variance_tex = ssr_reproject.AddStorageImageOutput(variance_tex_[1], Ren::eStageBits::ComputeShader);
        data->out_sample_count_tex =
            ssr_reproject.AddStorageImageOutput(sample_count_tex_[0], Ren::eStageBits::ComputeShader);

        ssr_reproject.set_execute_cb([this, data](RpBuilder &builder) {
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
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *shared_data_buf.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::DEPTH_HIST_TEX_SLOT, *depth_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::REFL_HIST_TEX_SLOT, *refl_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::REFL_TEX_SLOT, *relf_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRReproject::RAYLEN_TEX_SLOT, *raylen_tex.ref},
                {Ren::eBindTarget::SBuf, SSRReproject::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, SSRReproject::OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
                {Ren::eBindTarget::Image, SSRReproject::OUT_AVG_REFL_IMG_SLOT, *out_avg_refl_tex.ref},
                {Ren::eBindTarget::Image, SSRReproject::OUT_VERIANCE_IMG_SLOT, *out_variance_tex.ref},
                {Ren::eBindTarget::Image, SSRReproject::OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

            SSRReproject::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, 0.0f};

            Ren::DispatchComputeIndirect(pi_ssr_reproject_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef prefiltered_refl;

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
        data->depth_tex = ssr_prefilter.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = ssr_prefilter.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->avg_refl_tex = ssr_prefilter.AddTextureInput(avg_refl_tex, Ren::eStageBits::ComputeShader);
        data->refl_tex = ssr_prefilter.AddTextureInput(refl_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = ssr_prefilter.AddTextureInput(variance_tex_[1], Ren::eStageBits::ComputeShader);
        data->sample_count_tex = ssr_prefilter.AddTextureInput(sample_count_tex_[0], Ren::eStageBits::ComputeShader);
        data->tile_list = ssr_prefilter.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = ssr_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reflection color texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG11F_B10F;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::Storage);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            prefiltered_refl = data->out_refl_tex =
                ssr_prefilter.AddStorageImageOutput("SSR Denoised 1", params, Ren::eStageBits::ComputeShader);
        }

        data->out_variance_tex = ssr_prefilter.AddStorageImageOutput(variance_tex_[0], Ren::eStageBits::ComputeShader);

        ssr_prefilter.set_execute_cb([this, data](RpBuilder &builder) {
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

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, SSRPrefilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRPrefilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRPrefilter::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRPrefilter::REFL_TEX_SLOT, *refl_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRPrefilter::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRPrefilter::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::SBuf, SSRPrefilter::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},

                {Ren::eBindTarget::Image, SSRPrefilter::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Ren::eBindTarget::Image, SSRPrefilter::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            const auto grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + SSRPrefilter::LOCAL_GROUP_SIZE_X - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + SSRPrefilter::LOCAL_GROUP_SIZE_Y - 1u) / SSRPrefilter::LOCAL_GROUP_SIZE_Y,
                1u};

            SSRPrefilter::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, MirrorThreshold};

            Ren::DispatchComputeIndirect(pi_ssr_prefilter_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

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
        data->shared_data =
            ssr_temporal.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->norm_tex = ssr_temporal.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->avg_refl_tex = ssr_temporal.AddTextureInput(avg_refl_tex, Ren::eStageBits::ComputeShader);
        data->refl_tex = ssr_temporal.AddTextureInput(prefiltered_refl, Ren::eStageBits::ComputeShader);
        data->reproj_refl_tex = ssr_temporal.AddTextureInput(reproj_refl_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = ssr_temporal.AddTextureInput(variance_tex_[0], Ren::eStageBits::ComputeShader);
        data->sample_count_tex = ssr_temporal.AddTextureInput(sample_count_tex_[0], Ren::eStageBits::ComputeShader);
        data->tile_list = ssr_temporal.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = ssr_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        data->out_refl_tex = ssr_temporal.AddStorageImageOutput(refl_history_tex_, Ren::eStageBits::ComputeShader);
        data->out_variance_tex = ssr_temporal.AddStorageImageOutput(variance_tex_[1], Ren::eStageBits::ComputeShader);

        ssr_temporal.set_execute_cb([this, data](RpBuilder &builder) {
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
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::AVG_REFL_TEX_SLOT, *avg_refl_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REFL_TEX_SLOT, *refl_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::REPROJ_REFL_TEX_SLOT, *reproj_refl_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::Tex2D, SSRResolveTemporal::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::SBuf, SSRResolveTemporal::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, SSRResolveTemporal::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
                {Ren::eBindTarget::Image, SSRResolveTemporal::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            SSRResolveTemporal::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.thresholds = Ren::Vec2f{GlossyThreshold, MirrorThreshold};

            Ren::DispatchComputeIndirect(pi_ssr_resolve_temporal_, *indir_args_buf.ref, data->indir_args_offset,
                                         bindings, COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    { // Compose reflections
        auto &ssr_compose = rp_builder_.AddPass("SSR COMPOSE");

        auto *data = ssr_compose.AllocPassData<RpSSRCompose2Data>();

        data->shared_data = ssr_compose.AddUniformBufferInput(
            common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
        data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Ren::eStageBits::FragmentShader);
        data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Ren::eStageBits::FragmentShader);
        data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Ren::eStageBits::FragmentShader);

        if (debug_denoise) {
            data->refl_tex = ssr_compose.AddTextureInput(refl_tex, Ren::eStageBits::FragmentShader);
        } else {
            data->refl_tex = ssr_compose.AddTextureInput(refl_history_tex_, Ren::eStageBits::FragmentShader);
        }
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Ren::eStageBits::FragmentShader);
        frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

        rp_ssr_compose2_.Setup(&view_state_, probe_storage, data);
        ssr_compose.set_executor(&rp_ssr_compose2_);
    }

    { // copy history textures
        auto &copy_hist = rp_builder_.AddPass("SSR COPY HIST");

        struct PassData {
            RpResRef in_depth, in_normal;
            RpResRef out_depth, out_normal;

            RpResRef in_variance, in_sample_count;
            RpResRef out_variance, out_sample_count;
        };

        auto *data = copy_hist.AllocPassData<PassData>();

        data->in_depth = copy_hist.AddTransferImageInput(frame_textures.depth);
        data->in_normal = copy_hist.AddTransferImageInput(frame_textures.normal);
        data->in_variance = copy_hist.AddTransferImageInput(variance_tex_[0]);
        data->in_sample_count = copy_hist.AddTransferImageInput(sample_count_tex_[0]);

        data->out_depth = copy_hist.AddTransferImageOutput(depth_history_tex_);
        data->out_normal = copy_hist.AddTransferImageOutput(norm_history_tex_);
        data->out_variance = copy_hist.AddTransferImageOutput(variance_tex_[1]);
        data->out_sample_count = copy_hist.AddTransferImageOutput(sample_count_tex_[1]);

        // Make sure history copying pass will not be culled (temporary solution)
        backbuffer_sources_.push_back(data->out_depth);

        copy_hist.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &in_depth = builder.GetReadTexture(data->in_depth);
            RpAllocTex &in_normal = builder.GetReadTexture(data->in_normal);
            RpAllocTex &in_variance = builder.GetReadTexture(data->in_variance);
            RpAllocTex &in_sample_count = builder.GetReadTexture(data->in_sample_count);
            RpAllocTex &out_depth = builder.GetWriteTexture(data->out_depth);
            RpAllocTex &out_normal = builder.GetWriteTexture(data->out_normal);
            RpAllocTex &out_variance = builder.GetWriteTexture(data->out_variance);
            RpAllocTex &out_sample_count = builder.GetWriteTexture(data->out_sample_count);

            assert(in_depth.ref->params.format == out_depth.ref->params.format);
            Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *in_depth.ref, 0, 0, 0, *out_depth.ref, 0, 0, 0,
                                  view_state_.act_res[0], view_state_.act_res[1]);

            assert(in_normal.ref->params.format == out_normal.ref->params.format);
            Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *in_normal.ref, 0, 0, 0, *out_normal.ref, 0, 0, 0,
                                  view_state_.act_res[0], view_state_.act_res[1]);

            // TODO: avoid copying here
            assert(in_variance.ref->params.format == out_variance.ref->params.format);
            Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *in_variance.ref, 0, 0, 0, *out_variance.ref, 0, 0,
                                  0, view_state_.act_res[0], view_state_.act_res[1]);

            assert(in_sample_count.ref->params.format == out_sample_count.ref->params.format);
            Ren::CopyImageToImage(builder.ctx().current_cmd_buf(), *in_sample_count.ref, 0, 0, 0, *out_sample_count.ref,
                                  0, 0, 0, view_state_.act_res[0], view_state_.act_res[1]);
        });

        // std::swap(variance_tex_[0], variance_tex_[1]);
        // std::swap(sample_count_tex_[0], sample_count_tex_[1]);
    }
}

void Renderer::AddLQSpecularPasses(const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                   const RpResRef depth_down_2x, FrameTextures &frame_textures) {
    RpResRef ssr_temp1;
    { // Trace
        auto &ssr_trace = rp_builder_.AddPass("SSR TRACE");

        auto *data = ssr_trace.AllocPassData<RpSSRTraceData>();
        data->shared_data = ssr_trace.AddUniformBufferInput(
            common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
        data->normal_tex = ssr_trace.AddTextureInput(frame_textures.normal, Ren::eStageBits::FragmentShader);
        data->depth_down_2x_tex = ssr_trace.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawRGB10_A2;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssr_temp1 = data->output_tex = ssr_trace.AddColorOutput("SSR Temp 1", params);
        }

        rp_ssr_trace_.Setup(rp_builder_, &view_state_, data);
        ssr_trace.set_executor(&rp_ssr_trace_);
    }
    RpResRef ssr_temp2;
    { // Dilate
        auto &ssr_dilate = rp_builder_.AddPass("SSR DILATE");

        auto *data = ssr_dilate.AllocPassData<RpSSRDilateData>();
        data->ssr_tex = ssr_dilate.AddTextureInput(ssr_temp1, Ren::eStageBits::FragmentShader);

        { // Auxilary texture for reflections (rg - uvs, b - influence)
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::RawRGB10_A2;
            params.usage = (Ren::eTexUsage::Sampled | Ren::eTexUsage::RenderTarget);
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssr_temp2 = data->output_tex = ssr_dilate.AddColorOutput("SSR Temp 2", params);
        }

        rp_ssr_dilate_.Setup(rp_builder_, &view_state_, data);
        ssr_dilate.set_executor(&rp_ssr_dilate_);
    }
    { // Compose
        auto &ssr_compose = rp_builder_.AddPass("SSR COMPOSE");

        auto *data = ssr_compose.AllocPassData<RpSSRComposeData>();
        data->shared_data = ssr_compose.AddUniformBufferInput(
            common_buffers.shared_data_res, Ren::eStageBits::VertexShader | Ren::eStageBits::FragmentShader);
        data->cells_buf =
            ssr_compose.AddStorageReadonlyInput(common_buffers.cells_res, Ren::eStageBits::FragmentShader);
        data->items_buf =
            ssr_compose.AddStorageReadonlyInput(common_buffers.items_res, Ren::eStageBits::FragmentShader);

        data->depth_tex = ssr_compose.AddTextureInput(frame_textures.depth, Ren::eStageBits::FragmentShader);
        data->normal_tex = ssr_compose.AddTextureInput(frame_textures.normal, Ren::eStageBits::FragmentShader);
        data->spec_tex = ssr_compose.AddTextureInput(frame_textures.specular, Ren::eStageBits::FragmentShader);
        data->depth_down_2x_tex = ssr_compose.AddTextureInput(depth_down_2x, Ren::eStageBits::FragmentShader);
        data->down_buf_4x_tex = ssr_compose.AddTextureInput(down_tex_4x_, Ren::eStageBits::FragmentShader);
        data->ssr_tex = ssr_compose.AddTextureInput(ssr_temp2, Ren::eStageBits::FragmentShader);
        data->brdf_lut = ssr_compose.AddTextureInput(brdf_lut_, Ren::eStageBits::FragmentShader);

        frame_textures.color = data->output_tex = ssr_compose.AddColorOutput(frame_textures.color);

        rp_ssr_compose_.Setup(rp_builder_, &view_state_, probe_storage, data);
        ssr_compose.set_executor(&rp_ssr_compose_);
    }
}