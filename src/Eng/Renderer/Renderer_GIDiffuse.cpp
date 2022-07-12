#include "Renderer.h"

#include "../assets/shaders/internal/gi_blur_interface.glsl"
#include "../assets/shaders/internal/gi_classify_tiles_interface.glsl"
#include "../assets/shaders/internal/gi_prefilter_interface.glsl"
#include "../assets/shaders/internal/gi_reproject_interface.glsl"
#include "../assets/shaders/internal/gi_resolve_temporal_interface.glsl"
#include "../assets/shaders/internal/gi_trace_ss_interface.glsl"
#include "../assets/shaders/internal/gi_write_indir_rt_dispatch_interface.glsl"
#include "../assets/shaders/internal/gi_write_indirect_args_interface.glsl"
#include "../assets/shaders/internal/reconstruct_normals_interface.glsl"

void Renderer::AddDiffusePasses(const Ren::WeakTex2DRef &env_map, const Ren::WeakTex2DRef &lm_direct,
                                const Ren::WeakTex2DRef lm_indir_sh[4], const bool debug_denoise,
                                const Ren::ProbeStorage *probe_storage, const CommonBuffers &common_buffers,
                                const PersistentGpuData &persistent_data,
                                const AccelerationStructureData &acc_struct_data, const BindlessTextureData &bindless,
                                const RpResRef depth_hierarchy, FrameTextures &frame_textures) {
    // GI settings
    static const int SamplesPerQuad = 4;
    static const bool VarianceGuided = false;

    RpResRef flat_normals_tex;

    { // Prepare flat normals
        auto &flat_normals = rp_builder_.AddPass("FLAT NORMALS");

        struct PassData {
            RpResRef depth_tex;
            RpResRef out_normals_tex;
        };

        auto *data = flat_normals.AllocPassData<PassData>();

        data->depth_tex = flat_normals.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        { // normals texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRG16Snorm;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            flat_normals_tex = data->out_normals_tex =
                flat_normals.AddStorageImageOutput("Flat Normals", params, Ren::eStageBits::ComputeShader);
        }

        flat_normals.set_execute_cb([data, this](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &out_normals_tex = builder.GetWriteTexture(data->out_normals_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, ReconstructNormals::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Image, ReconstructNormals::OUT_NORMALS_IMG_SLOT, *out_normals_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + ReconstructNormals::LOCAL_GROUP_SIZE_X - 1u) /
                               ReconstructNormals::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + ReconstructNormals::LOCAL_GROUP_SIZE_Y - 1u) /
                               ReconstructNormals::LOCAL_GROUP_SIZE_Y,
                           1u};

            ReconstructNormals::Params uniform_params;
            uniform_params.frustum_info = view_state_.frustum_info;
            uniform_params.clip_info = view_state_.clip_info;
            uniform_params.img_size = Ren::Vec2u{view_state_.act_res};

            Ren::DispatchCompute(pi_reconstruct_normals_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                                 sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &gi_prepare = rp_builder_.AddPass("GI PREPARE");

        struct PassData {
            RpResRef ray_counter;
        };

        auto *data = gi_prepare.AllocPassData<PassData>();

        { // ray counter
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = 6 * sizeof(uint32_t);

            ray_counter = data->ray_counter = gi_prepare.AddTransferOutput("GI Ray Counter", desc);
        }

        gi_prepare.set_execute_cb([data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);

            Ren::Context &ctx = builder.ctx();
            ray_counter_buf.ref->Fill(0, ray_counter_buf.ref->size(), 0, ctx.current_cmd_buf());
        });
    }

    RpResRef ray_list, tile_list;
    RpResRef gi_tex, noise_tex;

    { // Classify pixel quads
        auto &gi_classify = rp_builder_.AddPass("GI CLASSIFY");

        struct PassData {
            RpResRef depth;
            RpResRef variance_history;
            RpResRef sobol, scrambling_tile, ranking_tile;
            RpResRef ray_counter;
            RpResRef ray_list;
            RpResRef tile_list;
            RpResRef out_gi_tex, out_noise_tex;
        };

        auto *data = gi_classify.AllocPassData<PassData>();
        data->depth = gi_classify.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->variance_history = gi_classify.AddHistoryTextureInput("GI Variance", Ren::eStageBits::ComputeShader);
        data->sobol = gi_classify.AddStorageReadonlyInput(sobol_seq_buf_, Ren::eStageBits::ComputeShader);
        data->scrambling_tile =
            gi_classify.AddStorageReadonlyInput(scrambling_tile_1spp_buf_, Ren::eStageBits::ComputeShader);
        data->ranking_tile =
            gi_classify.AddStorageReadonlyInput(ranking_tile_1spp_buf_, Ren::eStageBits::ComputeShader);
        ray_counter = data->ray_counter = gi_classify.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);

        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list =
                gi_classify.AddStorageOutput("GI Ray List", desc, Ren::eStageBits::ComputeShader);
        }
        { // tile list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 7) / 8) * sizeof(uint32_t);

            tile_list = data->tile_list =
                gi_classify.AddStorageOutput("GI Tile List", desc, Ren::eStageBits::ComputeShader);
        }
        { // gi texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            gi_tex = data->out_gi_tex =
                gi_classify.AddStorageImageOutput("GI Temp 2", params, Ren::eStageBits::ComputeShader);
        }
        { // blue noise texture
            Ren::Tex2DParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RawRG88;
            params.sampling.filter = Ren::eTexFilter::NoFilter;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex =
                gi_classify.AddStorageImageOutput("GI Blue Noise Tex", params, Ren::eStageBits::ComputeShader);
        }

        gi_classify.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_history);
            RpAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            RpAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            RpAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &ray_list_buf = builder.GetWriteBuffer(data->ray_list);
            RpAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            RpAllocTex &gi_tex = builder.GetWriteTexture(data->out_gi_tex);
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
                {Ren::eBindTarget::Tex2D, GIClassifyTiles::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, GIClassifyTiles::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::SBuf, GIClassifyTiles::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, GIClassifyTiles::RAY_LIST_SLOT, *ray_list_buf.ref},
                {Ren::eBindTarget::SBuf, GIClassifyTiles::TILE_LIST_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::TBuf, GIClassifyTiles::SOBOL_BUF_SLOT, *sobol_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, GIClassifyTiles::SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.tbos[0]},
                {Ren::eBindTarget::TBuf, GIClassifyTiles::RANKING_TILE_BUF_SLOT, *ranking_tile_buf.tbos[0]},
                {Ren::eBindTarget::Image, GIClassifyTiles::GI_IMG_SLOT, *gi_tex.ref},
                {Ren::eBindTarget::Image, GIClassifyTiles::NOISE_IMG_SLOT, *noise_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + GIClassifyTiles::LOCAL_GROUP_SIZE_X - 1u) /
                               GIClassifyTiles::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + GIClassifyTiles::LOCAL_GROUP_SIZE_Y - 1u) /
                               GIClassifyTiles::LOCAL_GROUP_SIZE_Y,
                           1u};

            GIClassifyTiles::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.samples_and_guided = Ren::Vec2u{uint32_t(SamplesPerQuad), VarianceGuided ? 1u : 0u};
            uniform_params.frame_index = view_state_.frame_index;

            Ren::DispatchCompute(pi_gi_classify_tiles_, grp_count, bindings, COUNT_OF(bindings), &uniform_params,
                                 sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef indir_disp_buf;

    { // Write indirect arguments
        auto &write_indir = rp_builder_.AddPass("GI INDIR ARGS");

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
                write_indir.AddStorageOutput("GI Intersect Args", desc, Ren::eStageBits::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            RpAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::SBuf, GIWriteIndirectArgs::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, GIWriteIndirectArgs::INDIR_ARGS_SLOT, *indir_args.ref}};

            Ren::DispatchCompute(pi_gi_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, COUNT_OF(bindings), nullptr,
                                 0, builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef ray_rt_list;

    { // Trace screen-space rays
        auto &gi_trace_ss = rp_builder_.AddPass("GI TRACE SS");

        struct PassData {
            RpResRef noise_tex;
            RpResRef shared_data;
            RpResRef color_tex, normal_tex, depth_hierarchy;

            RpResRef in_ray_list, indir_args, inout_ray_counter;
            RpResRef out_gi_tex, out_ray_list;
        };

        auto *data = gi_trace_ss.AllocPassData<PassData>();
        data->noise_tex = gi_trace_ss.AddTextureInput(noise_tex, Ren::eStageBits::ComputeShader);
        data->shared_data =
            gi_trace_ss.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->color_tex = gi_trace_ss.AddHistoryTextureInput(MAIN_COLOR_TEX, Ren::eStageBits::ComputeShader);
        data->normal_tex = gi_trace_ss.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->depth_hierarchy = gi_trace_ss.AddTextureInput(depth_hierarchy, Ren::eStageBits::ComputeShader);

        data->in_ray_list = gi_trace_ss.AddStorageReadonlyInput(ray_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_trace_ss.AddIndirectBufferInput(indir_disp_buf);
        ray_counter = data->inout_ray_counter =
            gi_trace_ss.AddStorageOutput(ray_counter, Ren::eStageBits::ComputeShader);
        gi_tex = data->out_gi_tex = gi_trace_ss.AddStorageImageOutput(gi_tex, Ren::eStageBits::ComputeShader);

        { // packed ray list
            RpBufDesc desc;
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list =
                gi_trace_ss.AddStorageOutput("GI RT Ray List", desc, Ren::eStageBits::ComputeShader);
        }

        gi_trace_ss.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &noise_tex = builder.GetReadTexture(data->noise_tex);
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &color_tex = builder.GetReadTexture(data->color_tex);
            RpAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            RpAllocTex &depth_hierarchy_tex = builder.GetReadTexture(data->depth_hierarchy);
            RpAllocBuf &in_ray_list_buf = builder.GetReadBuffer(data->in_ray_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            RpAllocBuf &inout_ray_counter_buf = builder.GetWriteBuffer(data->inout_ray_counter);
            RpAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(data->out_ray_list);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, GITraceSS::DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                {Ren::eBindTarget::Tex2D, GITraceSS::COLOR_TEX_SLOT, *color_tex.ref},
                {Ren::eBindTarget::Tex2D, GITraceSS::NORM_TEX_SLOT, *normal_tex.ref},
                {Ren::eBindTarget::Tex2D, GITraceSS::NOISE_TEX_SLOT, *noise_tex.ref},
                {Ren::eBindTarget::SBuf, GITraceSS::IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                {Ren::eBindTarget::Image, GITraceSS::OUT_GI_IMG_SLOT, *out_gi_tex.ref},
                {Ren::eBindTarget::SBuf, GITraceSS::INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                {Ren::eBindTarget::SBuf, GITraceSS::OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};

            GITraceSS::Params uniform_params;
            uniform_params.resolution =
                Ren::Vec4u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1]), 0, 0};

            Ren::DispatchComputeIndirect(pi_gi_trace_ss_, *indir_args_buf.ref, 0, bindings, COUNT_OF(bindings),
                                         &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                         builder.ctx().log());
        });
    }

    if (ctx_.capabilities.raytracing && env_map) {
        RpResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = rp_builder_.AddPass("GI RT DISP ARGS");

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
                    rt_disp_args.AddStorageOutput("GI RT Dispatch Args", desc, Ren::eStageBits::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](RpBuilder &builder) {
                RpAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                RpAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {
                    {Ren::eBindTarget::SBuf, GIWriteIndirRTDispatch::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                    {Ren::eBindTarget::SBuf, GIWriteIndirRTDispatch::INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                Ren::DispatchCompute(pi_gi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, COUNT_OF(bindings),
                                     nullptr, 0, builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace gi rays
            auto &rt_gi = rp_builder_.AddPass("RT GI");

            auto *data = rt_gi.AllocPassData<RpRTGIData>();

            const Ren::eStageBits stage =
                ctx_.capabilities.ray_query ? Ren::eStageBits::ComputeShader : Ren::eStageBits::RayTracingShader;

            data->geo_data = rt_gi.AddStorageReadonlyInput(acc_struct_data.rt_geo_data_buf, stage);
            data->materials = rt_gi.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_gi.AddStorageReadonlyInput(ctx_.default_vertex_buf1(), stage);
            data->vtx_buf2 = rt_gi.AddStorageReadonlyInput(ctx_.default_vertex_buf2(), stage);
            data->ndx_buf = rt_gi.AddStorageReadonlyInput(ctx_.default_indices_buf(), stage);
            data->shared_data = rt_gi.AddUniformBufferInput(common_buffers.shared_data_res, stage);
            data->noise_tex = rt_gi.AddTextureInput(noise_tex, stage);
            data->depth_tex = rt_gi.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_gi.AddTextureInput(frame_textures.normal, stage);
            // data->flat_normal_tex = rt_gi.AddTextureInput(flat_normals_tex, stage);
            data->env_tex = rt_gi.AddTextureInput(env_map, stage);
            data->ray_counter = rt_gi.AddStorageReadonlyInput(ray_counter, stage);
            data->ray_list = rt_gi.AddStorageReadonlyInput(ray_rt_list, stage);
            data->indir_args = rt_gi.AddIndirectBufferInput(indir_rt_disp_buf);
            data->tlas_buf = rt_gi.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);

            if (lm_direct) {
                data->lm_tex[0] = rt_gi.AddTextureInput(lm_direct, stage);
            }

            for (int i = 0; i < 4; ++i) {
                if (lm_indir_sh[i]) {
                    data->lm_tex[i + 1] = rt_gi.AddTextureInput(lm_indir_sh[i], stage);
                }
            }

            data->dummy_black = rt_gi.AddTextureInput(dummy_black_, stage);

            gi_tex = data->out_gi_tex = rt_gi.AddStorageImageOutput(gi_tex, stage);

            rp_rt_gi_.Setup(rp_builder_, &view_state_, &acc_struct_data, &bindless, data);
            rt_gi.set_executor(&rp_rt_gi_);
        }
    }

    if (debug_denoise) {
        frame_textures.gi = gi_tex;
        return;
    }

    RpResRef reproj_gi_tex, avg_gi_tex;
    RpResRef variance_temp_tex, sample_count_tex;

    { // Denoiser reprojection
        auto &gi_reproject = rp_builder_.AddPass("GI REPROJECT");

        struct PassData {
            RpResRef shared_data;
            RpResRef depth_tex, norm_tex, velocity_tex;
            RpResRef depth_hist_tex, norm_hist_tex, gi_hist_tex, variance_hist_tex, sample_count_hist_tex;
            RpResRef gi_tex;
            RpResRef tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_reprojected_tex, out_avg_gi_tex;
            RpResRef out_variance_tex, out_sample_count_tex;
        };

        auto *data = gi_reproject.AllocPassData<PassData>();
        data->shared_data =
            gi_reproject.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->depth_tex = gi_reproject.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = gi_reproject.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->velocity_tex = gi_reproject.AddTextureInput(frame_textures.velocity, Ren::eStageBits::ComputeShader);
        data->depth_hist_tex =
            gi_reproject.AddHistoryTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_hist_tex =
            gi_reproject.AddHistoryTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->gi_hist_tex = gi_reproject.AddHistoryTextureInput("GI Diffuse 3", Ren::eStageBits::ComputeShader);
        data->variance_hist_tex = gi_reproject.AddHistoryTextureInput("GI Variance", Ren::eStageBits::ComputeShader);
        data->gi_tex = gi_reproject.AddTextureInput(gi_tex, Ren::eStageBits::ComputeShader);

        data->tile_list = gi_reproject.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Reprojected reflections texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            reproj_gi_tex = data->out_reprojected_tex =
                gi_reproject.AddStorageImageOutput("GI Reprojected", params, Ren::eStageBits::ComputeShader);
        }
        { // 8x8 average reflections texture
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 7) / 8;
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            avg_gi_tex = data->out_avg_gi_tex =
                gi_reproject.AddStorageImageOutput("Average GI", params, Ren::eStageBits::ComputeShader);
        }
        { // Variance
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            variance_temp_tex = data->out_variance_tex =
                gi_reproject.AddStorageImageOutput("GI Variance Temp", params, Ren::eStageBits::ComputeShader);
        }
        { // Sample count
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sample_count_tex = data->out_sample_count_tex =
                gi_reproject.AddStorageImageOutput("GI Sample Count", params, Ren::eStageBits::ComputeShader);
        }

        data->sample_count_hist_tex =
            gi_reproject.AddHistoryTextureInput(data->out_sample_count_tex, Ren::eStageBits::ComputeShader);

        gi_reproject.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
            RpAllocTex &depth_hist_tex = builder.GetReadTexture(data->depth_hist_tex);
            RpAllocTex &norm_hist_tex = builder.GetReadTexture(data->norm_hist_tex);
            RpAllocTex &gi_hist_tex = builder.GetReadTexture(data->gi_hist_tex);
            RpAllocTex &variance_hist_tex = builder.GetReadTexture(data->variance_hist_tex);
            RpAllocTex &sample_count_hist_tex = builder.GetReadTexture(data->sample_count_hist_tex);
            RpAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);
            RpAllocTex &out_reprojected_tex = builder.GetWriteTexture(data->out_reprojected_tex);
            RpAllocTex &out_avg_gi_tex = builder.GetWriteTexture(data->out_avg_gi_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);
            RpAllocTex &out_sample_count_tex = builder.GetWriteTexture(data->out_sample_count_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *shared_data_buf.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::DEPTH_HIST_TEX_SLOT, *depth_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::GI_HIST_TEX_SLOT, *gi_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
                {Ren::eBindTarget::Tex2D, GIReproject::GI_TEX_SLOT, *gi_tex.ref},
                {Ren::eBindTarget::SBuf, GIReproject::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, GIReproject::OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
                {Ren::eBindTarget::Image, GIReproject::OUT_AVG_GI_IMG_SLOT, *out_avg_gi_tex.ref},
                {Ren::eBindTarget::Image, GIReproject::OUT_VERIANCE_IMG_SLOT, *out_variance_tex.ref},
                {Ren::eBindTarget::Image, GIReproject::OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

            GIReproject::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_gi_reproject_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef prefiltered_gi, variance_temp2_tex;

    { // Denoiser prefilter
        auto &gi_prefilter = rp_builder_.AddPass("GI PREFILTER");

        struct PassData {
            RpResRef depth_tex, norm_tex;
            RpResRef avg_gi_tex, gi_tex;
            RpResRef variance_tex, sample_count_tex;
            RpResRef tile_list, indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_gi_tex, out_variance_tex;
        };

        auto *data = gi_prefilter.AllocPassData<PassData>();
        data->depth_tex = gi_prefilter.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = gi_prefilter.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->avg_gi_tex = gi_prefilter.AddTextureInput(avg_gi_tex, Ren::eStageBits::ComputeShader);
        data->gi_tex = gi_prefilter.AddTextureInput(gi_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = gi_prefilter.AddTextureInput(variance_temp_tex, Ren::eStageBits::ComputeShader);
        data->sample_count_tex = gi_prefilter.AddTextureInput(sample_count_tex, Ren::eStageBits::ComputeShader);
        data->tile_list = gi_prefilter.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // GI color texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            prefiltered_gi = data->out_gi_tex =
                gi_prefilter.AddStorageImageOutput("GI Denoised 1", params, Ren::eStageBits::ComputeShader);
        }
        { // Variance
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            variance_temp2_tex = data->out_variance_tex =
                gi_prefilter.AddStorageImageOutput("GI Variance Temp 2", params, Ren::eStageBits::ComputeShader);
        }

        gi_prefilter.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &avg_gi_tex = builder.GetReadTexture(data->avg_gi_tex);
            RpAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2D, GIPrefilter::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, GIPrefilter::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, GIPrefilter::AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIPrefilter::GI_TEX_SLOT, *gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIPrefilter::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::Tex2D, GIPrefilter::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::SBuf, GIPrefilter::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},

                {Ren::eBindTarget::Image, GIPrefilter::OUT_GI_IMG_SLOT, *out_gi_tex.ref},
                {Ren::eBindTarget::Image, GIPrefilter::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            const auto grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + GIPrefilter::LOCAL_GROUP_SIZE_X - 1u) / GIPrefilter::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + GIPrefilter::LOCAL_GROUP_SIZE_Y - 1u) / GIPrefilter::LOCAL_GROUP_SIZE_Y, 1u};

            GIPrefilter::Params uniform_params;
            uniform_params.rotator = view_state_.rand_rotators[0];
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_gi_prefilter_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef gi_diffuse_tex, gi_variance_tex;

    { // Denoiser accumulation
        auto &gi_temporal = rp_builder_.AddPass("GI TEMPORAL");

        struct PassData {
            RpResRef shared_data;
            RpResRef norm_tex, avg_gi_tex, gi_tex, reproj_gi_tex;
            RpResRef variance_tex, sample_count_tex, tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_gi_tex, out_variance_tex;
        };

        auto *data = gi_temporal.AllocPassData<PassData>();
        data->shared_data =
            gi_temporal.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->norm_tex = gi_temporal.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->avg_gi_tex = gi_temporal.AddTextureInput(avg_gi_tex, Ren::eStageBits::ComputeShader);
        data->gi_tex = gi_temporal.AddTextureInput(prefiltered_gi, Ren::eStageBits::ComputeShader);
        data->reproj_gi_tex = gi_temporal.AddTextureInput(reproj_gi_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = gi_temporal.AddTextureInput(variance_temp2_tex, Ren::eStageBits::ComputeShader);
        data->sample_count_tex = gi_temporal.AddTextureInput(sample_count_tex, Ren::eStageBits::ComputeShader);
        data->tile_list = gi_temporal.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Final gi
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_diffuse_tex = data->out_gi_tex =
                gi_temporal.AddStorageImageOutput("GI Diffuse", params, Ren::eStageBits::ComputeShader);
        }
        { // Variance texture
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawR16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_variance_tex = data->out_variance_tex =
                gi_temporal.AddStorageImageOutput("GI Variance", params, Ren::eStageBits::ComputeShader);
        }

        gi_temporal.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &avg_gi_tex = builder.GetReadTexture(data->avg_gi_tex);
            RpAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            RpAllocTex &reproj_gi_tex = builder.GetReadTexture(data->reproj_gi_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            RpAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::NORM_TEX_SLOT, *norm_tex.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::GI_TEX_SLOT, *gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::REPROJ_GI_TEX_SLOT, *reproj_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::Tex2D, GIResolveTemporal::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::SBuf, GIResolveTemporal::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, GIResolveTemporal::OUT_GI_IMG_SLOT, *out_gi_tex.ref},
                {Ren::eBindTarget::Image, GIResolveTemporal::OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

            GIResolveTemporal::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};

            Ren::DispatchComputeIndirect(pi_gi_resolve_temporal_, *indir_args_buf.ref, data->indir_args_offset,
                                         bindings, COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef gi_diffuse2_tex;

    { // Denoiser blur
        auto &gi_blur = rp_builder_.AddPass("GI BLUR");

        struct PassData {
            RpResRef shared_data;
            RpResRef depth_tex, norm_tex, gi_tex;
            RpResRef sample_count_tex, variance_tex, tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_gi_tex;
        };

        auto *data = gi_blur.AllocPassData<PassData>();
        data->shared_data =
            gi_blur.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->depth_tex = gi_blur.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = gi_blur.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->gi_tex = gi_blur.AddTextureInput(gi_diffuse_tex, Ren::eStageBits::ComputeShader);
        data->sample_count_tex = gi_blur.AddTextureInput(sample_count_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = gi_blur.AddTextureInput(gi_variance_tex, Ren::eStageBits::ComputeShader);
        data->tile_list = gi_blur.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_blur.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Final gi
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_diffuse2_tex = data->out_gi_tex =
                gi_blur.AddStorageImageOutput("GI Diffuse 2", params, Ren::eStageBits::ComputeShader);
        }

        gi_blur.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::NORM_TEX_SLOT, *norm_tex.ref},
                //{Ren::eBindTarget::Tex2D, GIBlur::AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::GI_TEX_SLOT, *gi_tex.ref},
                //{Ren::eBindTarget::Tex2D, GIBlur::REPROJ_GI_TEX_SLOT, *reproj_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::SBuf, GIBlur::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, GIBlur::OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

            GIBlur::Params uniform_params;
            uniform_params.rotator = view_state_.rand_rotators[1];
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

            Ren::DispatchComputeIndirect(pi_gi_blur_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    RpResRef gi_diffuse3_tex;

    { // Denoiser blur 2
        auto &gi_post_blur = rp_builder_.AddPass("GI POST BLUR");

        struct PassData {
            RpResRef shared_data;
            RpResRef depth_tex, norm_tex, gi_tex, raylen_tex;
            RpResRef sample_count_tex, variance_tex, tile_list;
            RpResRef indir_args;
            uint32_t indir_args_offset = 0;
            RpResRef out_gi_tex;
        };

        auto *data = gi_post_blur.AllocPassData<PassData>();
        data->shared_data =
            gi_post_blur.AddUniformBufferInput(common_buffers.shared_data_res, Ren::eStageBits::ComputeShader);
        data->depth_tex = gi_post_blur.AddTextureInput(frame_textures.depth, Ren::eStageBits::ComputeShader);
        data->norm_tex = gi_post_blur.AddTextureInput(frame_textures.normal, Ren::eStageBits::ComputeShader);
        data->gi_tex = gi_post_blur.AddTextureInput(gi_diffuse2_tex, Ren::eStageBits::ComputeShader);
        data->sample_count_tex = gi_post_blur.AddTextureInput(sample_count_tex, Ren::eStageBits::ComputeShader);
        data->variance_tex = gi_post_blur.AddTextureInput(gi_variance_tex, Ren::eStageBits::ComputeShader);
        data->tile_list = gi_post_blur.AddStorageReadonlyInput(tile_list, Ren::eStageBits::ComputeShader);
        data->indir_args = gi_post_blur.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset = 3 * sizeof(uint32_t);

        { // Final gi
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RawRGBA16F;
            params.sampling.filter = Ren::eTexFilter::BilinearNoMipmap;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_diffuse3_tex = data->out_gi_tex =
                gi_post_blur.AddStorageImageOutput("GI Diffuse 3", params, Ren::eStageBits::ComputeShader);
        }

        gi_post_blur.set_execute_cb([this, data](RpBuilder &builder) {
            RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            RpAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            RpAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            RpAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            RpAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            RpAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            RpAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            RpAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            RpAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::DEPTH_TEX_SLOT, *depth_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::NORM_TEX_SLOT, *norm_tex.ref},
                //{Ren::eBindTarget::Tex2D, GIBlur::AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::GI_TEX_SLOT, *gi_tex.ref},
                //{Ren::eBindTarget::Tex2D, GIBlur::REPROJ_GI_TEX_SLOT, *reproj_gi_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                {Ren::eBindTarget::Tex2D, GIBlur::VARIANCE_TEX_SLOT, *variance_tex.ref},
                {Ren::eBindTarget::SBuf, GIBlur::TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                {Ren::eBindTarget::Image, GIBlur::OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

            GIBlur::Params uniform_params;
            uniform_params.rotator = view_state_.rand_rotators[2];
            uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_.act_res[0]), uint32_t(view_state_.act_res[1])};
            uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

            Ren::DispatchComputeIndirect(pi_gi_post_blur_, *indir_args_buf.ref, data->indir_args_offset, bindings,
                                         COUNT_OF(bindings), &uniform_params, sizeof(uniform_params),
                                         builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    frame_textures.gi = gi_diffuse3_tex;
}
