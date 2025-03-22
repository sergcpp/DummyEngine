#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"

#include "shaders/blit_bilateral_interface.h"
#include "shaders/blit_ssao_interface.h"
#include "shaders/blit_upscale_interface.h"
#include "shaders/gi_classify_interface.h"
#include "shaders/gi_filter_interface.h"
#include "shaders/gi_reproject_interface.h"
#include "shaders/gi_stabilization_interface.h"
#include "shaders/gi_temporal_interface.h"
#include "shaders/gi_trace_ss_interface.h"
#include "shaders/gi_write_indir_rt_dispatch_interface.h"
#include "shaders/gi_write_indirect_args_interface.h"
#include "shaders/gtao_interface.h"
#include "shaders/probe_sample_interface.h"
#include "shaders/tile_clear_interface.h"

namespace RendererInternal {
static const float GTAORandSamples[32][2] = {
    {0.673997f, 0.678703f}, {0.381107f, 0.299157f}, {0.830422f, 0.123435f}, {0.110746f, 0.927141f},
    {0.913511f, 0.797823f}, {0.160077f, 0.141460f}, {0.557984f, 0.453895f}, {0.323667f, 0.502007f},
    {0.597559f, 0.967611f}, {0.284918f, 0.020696f}, {0.943984f, 0.367100f}, {0.228963f, 0.742073f},
    {0.794414f, 0.611045f}, {0.025854f, 0.406871f}, {0.695394f, 0.243437f}, {0.476599f, 0.826670f},
    {0.502447f, 0.539025f}, {0.353506f, 0.469774f}, {0.895886f, 0.159506f}, {0.131713f, 0.774900f},
    {0.857590f, 0.887409f}, {0.067422f, 0.090030f}, {0.648406f, 0.253075f}, {0.435085f, 0.632276f},
    {0.743669f, 0.861615f}, {0.442667f, 0.211039f}, {0.759943f, 0.403822f}, {0.037858f, 0.585661f},
    {0.978491f, 0.693668f}, {0.195222f, 0.323286f}, {0.566908f, 0.055406f}, {0.256133f, 0.988877f}};
}

void Eng::Renderer::AddDiffusePasses(const Ren::WeakTexRef &env_map, const Ren::WeakTexRef &lm_direct,
                                     const Ren::WeakTexRef lm_indir_sh[4], const bool debug_denoise,
                                     const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const AccelerationStructureData &acc_struct_data,
                                     const BindlessTextureData &bindless, const FgResRef depth_hierarchy,
                                     FgResRef rt_geo_instances_res, FgResRef rt_obj_instances_res,
                                     FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    FgResRef gi_fallback;

    if (frame_textures.gi_cache_irradiance) {
        auto &probe_sample = fg_builder_.AddNode("PROBE SAMPLE");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex;
            FgResRef normals_tex;
            FgResRef ssao_tex;
            FgResRef irradiance_tex;
            FgResRef distance_tex;
            FgResRef offset_tex;
            FgResRef out_tex;
        };

        auto *data = probe_sample.AllocNodeData<PassData>();

        data->shared_data = probe_sample.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth_tex = probe_sample.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normals_tex = probe_sample.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->ssao_tex = probe_sample.AddTextureInput(frame_textures.ssao, Stg::ComputeShader);
        data->irradiance_tex = probe_sample.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
        data->distance_tex = probe_sample.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
        data->offset_tex = probe_sample.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        { // gi texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_fallback = data->out_tex = probe_sample.AddStorageImageOutput("GI Tex", params, Stg::ComputeShader);
        }

        probe_sample.set_execute_cb([data, &persistent_data, this](FgBuilder &builder) {
            using namespace ProbeSample;

            FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &normals_tex = builder.GetReadTexture(data->normals_tex);
            FgAllocTex &ssao_tex = builder.GetReadTexture(data->ssao_tex);
            FgAllocTex &irr_tex = builder.GetReadTexture(data->irradiance_tex);
            FgAllocTex &dist_tex = builder.GetReadTexture(data->distance_tex);
            FgAllocTex &off_tex = builder.GetReadTexture(data->offset_tex);
            FgAllocTex &out_tex = builder.GetWriteTexture(data->out_tex);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
                {Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Trg::Tex2DSampled, NORM_TEX_SLOT, *normals_tex.ref},
                {Trg::Tex2DSampled, SSAO_TEX_SLOT, *ssao_tex.ref},
                {Trg::Tex2DArraySampled, IRRADIANCE_TEX_SLOT, *irr_tex.ref},
                {Trg::Tex2DArraySampled, DISTANCE_TEX_SLOT, *dist_tex.ref},
                {Trg::Tex2DArraySampled, OFFSET_TEX_SLOT, *off_tex.ref},
                {Trg::Image, OUT_IMG_SLOT, *out_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + LOCAL_GROUP_SIZE_X - 1u) / LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + LOCAL_GROUP_SIZE_Y - 1u) / LOCAL_GROUP_SIZE_Y, 1u};

            const Ren::Vec3f &grid_origin = persistent_data.probe_volumes[0].origin;
            const Ren::Vec3i &grid_scroll = persistent_data.probe_volumes[0].scroll;
            const Ren::Vec3f &grid_spacing = persistent_data.probe_volumes[0].spacing;

            Params uniform_params;
            uniform_params.grid_origin = Ren::Vec4f{grid_origin[0], grid_origin[1], grid_origin[2], 0.0f};
            uniform_params.grid_scroll = Ren::Vec4i{grid_scroll[0], grid_scroll[1], grid_scroll[2], 0};
            uniform_params.grid_spacing = Ren::Vec4f{grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f};
            uniform_params.img_size = Ren::Vec2u{view_state_.act_res};

            DispatchCompute(*pi_probe_sample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    } else {
        gi_fallback = fg_builder_.MakeTextureResource(dummy_black_);
    }

    if (settings.gi_quality <= eGIQuality::Medium) {
        frame_textures.gi = gi_fallback;
        return;
    }

    // GI settings
    const int SamplesPerQuad = (settings.gi_quality == eGIQuality::Ultra) ? 4 : 1;
    static const bool VarianceGuided = true;
    static const bool EnableBlur = true;
    const bool EnableStabilization = false;
    //(settings.taa_mode != eTAAMode::Static);

    FgResRef ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &gi_prepare = fg_builder_.AddNode("GI PREPARE");

        struct PassData {
            FgResRef ray_counter;
        };

        auto *data = gi_prepare.AllocNodeData<PassData>();

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 8 * sizeof(uint32_t);

            ray_counter = data->ray_counter = gi_prepare.AddTransferOutput("GI Ray Counter", desc);
        }

        gi_prepare.set_execute_cb([data](FgBuilder &builder) {
            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);

            Ren::Context &ctx = builder.ctx();
            ray_counter_buf.ref->Fill(0, ray_counter_buf.ref->size(), 0, ctx.current_cmd_buf());
        });
    }

    const int tile_count = ((view_state_.scr_res[0] + 7) / 8) * ((view_state_.scr_res[1] + 7) / 8);

    FgResRef ray_list, tile_list;
    FgResRef gi_tex, noise_tex;

    { // Classify pixel quads
        auto &gi_classify = fg_builder_.AddNode("GI CLASSIFY");

        struct PassData {
            FgResRef depth;
            FgResRef spec_tex;
            FgResRef variance_history;
            FgResRef sobol, scrambling_tile, ranking_tile;
            FgResRef ray_counter;
            FgResRef ray_list;
            FgResRef tile_list;
            FgResRef out_gi_tex, out_noise_tex;
        };

        auto *data = gi_classify.AllocNodeData<PassData>();
        data->depth = gi_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->spec_tex = gi_classify.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        if (debug_denoise) {
            data->variance_history = gi_classify.AddTextureInput(dummy_black_, Stg::ComputeShader);
        } else {
            data->variance_history = gi_classify.AddHistoryTextureInput(DIFFUSE_VARIANCE_TEX, Stg::ComputeShader);
        }
        data->sobol = gi_classify.AddStorageReadonlyInput(sobol_seq_buf_, Stg::ComputeShader);
        data->scrambling_tile = gi_classify.AddStorageReadonlyInput(scrambling_tile_buf_, Stg::ComputeShader);
        data->ranking_tile = gi_classify.AddStorageReadonlyInput(ranking_tile_buf_, Stg::ComputeShader);
        ray_counter = data->ray_counter = gi_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = gi_classify.AddStorageOutput("GI Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = tile_count * sizeof(uint32_t);

            tile_list = data->tile_list = gi_classify.AddStorageOutput("GI Tile List", desc, Stg::ComputeShader);
        }
        { // final gi texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
            gi_tex = data->out_gi_tex = gi_classify.AddStorageImageOutput("GI Final", params, Stg::ComputeShader);
        }
        { // blue noise texture
            Ren::TexParams params;
            params.w = params.h = 128;
            params.format = Ren::eTexFormat::RGBA8;
            params.sampling.filter = Ren::eTexFilter::Nearest;
            params.sampling.wrap = Ren::eTexWrap::Repeat;
            noise_tex = data->out_noise_tex =
                gi_classify.AddStorageImageOutput("GI BN Tex", params, Stg::ComputeShader);
        }

        gi_classify.set_execute_cb([this, data, tile_count, SamplesPerQuad](FgBuilder &builder) {
            using namespace GIClassifyTiles;

            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth);
            FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
            FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_history);
            FgAllocBuf &sobol_buf = builder.GetReadBuffer(data->sobol);
            FgAllocBuf &scrambling_tile_buf = builder.GetReadBuffer(data->scrambling_tile);
            FgAllocBuf &ranking_tile_buf = builder.GetReadBuffer(data->ranking_tile);

            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            FgAllocBuf &ray_list_buf = builder.GetWriteBuffer(data->ray_list);
            FgAllocBuf &tile_list_buf = builder.GetWriteBuffer(data->tile_list);
            FgAllocTex &gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            FgAllocTex &noise_tex = builder.GetWriteTexture(data->out_noise_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, SPEC_TEX_SLOT, *spec_tex.ref},
                                             {Trg::Tex2DSampled, VARIANCE_TEX_SLOT, *variance_tex.ref},
                                             {Trg::SBufRO, RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                             {Trg::SBufRO, RAY_LIST_SLOT, *ray_list_buf.ref},
                                             {Trg::SBufRO, TILE_LIST_SLOT, *tile_list_buf.ref},
                                             {Trg::UTBuf, SOBOL_BUF_SLOT, *sobol_buf.ref},
                                             {Trg::UTBuf, SCRAMLING_TILE_BUF_SLOT, *scrambling_tile_buf.ref},
                                             {Trg::UTBuf, RANKING_TILE_BUF_SLOT, *ranking_tile_buf.ref},
                                             {Trg::Image, GI_IMG_SLOT, *gi_tex.ref},
                                             {Trg::Image, NOISE_IMG_SLOT, *noise_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + LOCAL_GROUP_SIZE_X - 1u) / LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + LOCAL_GROUP_SIZE_Y - 1u) / LOCAL_GROUP_SIZE_Y, 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
            uniform_params.samples_and_guided = Ren::Vec2u{uint32_t(SamplesPerQuad), VarianceGuided ? 1u : 0u};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.tile_count = tile_count;

            DispatchCompute(*pi_gi_classify_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef indir_disp_buf;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("GI INDIR ARGS");

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
                write_indir.AddStorageOutput("GI Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](FgBuilder &builder) {
            using namespace GIWriteIndirectArgs;

            FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
            FgAllocBuf &indir_args = builder.GetWriteBuffer(data->indir_disp_buf);

            const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                             {Trg::SBufRW, INDIR_ARGS_SLOT, *indir_args.ref}};

            DispatchCompute(*pi_gi_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    FgResRef ray_rt_list;

    { // Trace screen-space rays
        auto &gi_trace_ss = fg_builder_.AddNode("GI TRACE SS");

        struct PassData {
            FgResRef noise_tex;
            FgResRef shared_data;
            FgResRef color_tex, normal_tex, depth_hierarchy;

            FgResRef in_ray_list, indir_args, inout_ray_counter;
            FgResRef out_gi_tex, out_ray_list;
        };

        auto *data = gi_trace_ss.AllocNodeData<PassData>();
        data->noise_tex = gi_trace_ss.AddTextureInput(noise_tex, Stg::ComputeShader);
        data->shared_data = gi_trace_ss.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->color_tex = gi_trace_ss.AddHistoryTextureInput(MAIN_COLOR_TEX, Stg::ComputeShader);
        data->normal_tex = gi_trace_ss.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->depth_hierarchy = gi_trace_ss.AddTextureInput(depth_hierarchy, Stg::ComputeShader);

        data->in_ray_list = gi_trace_ss.AddStorageReadonlyInput(ray_list, Stg::ComputeShader);
        data->indir_args = gi_trace_ss.AddIndirectBufferInput(indir_disp_buf);
        ray_counter = data->inout_ray_counter = gi_trace_ss.AddStorageOutput(ray_counter, Stg::ComputeShader);
        gi_tex = data->out_gi_tex = gi_trace_ss.AddStorageImageOutput(gi_tex, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.scr_res[0] * view_state_.scr_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list = gi_trace_ss.AddStorageOutput("GI RT Ray List", desc, Stg::ComputeShader);
        }

        gi_trace_ss.set_execute_cb([this, data](FgBuilder &builder) {
            using namespace GITraceSS;

            FgAllocTex &noise_tex = builder.GetReadTexture(data->noise_tex);
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &color_tex = builder.GetReadTexture(data->color_tex);
            FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
            FgAllocTex &depth_hierarchy_tex = builder.GetReadTexture(data->depth_hierarchy);
            FgAllocBuf &in_ray_list_buf = builder.GetReadBuffer(data->in_ray_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            FgAllocBuf &inout_ray_counter_buf = builder.GetWriteBuffer(data->inout_ray_counter);
            FgAllocBuf &out_ray_list_buf = builder.GetWriteBuffer(data->out_ray_list);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::Tex2DSampled, DEPTH_TEX_SLOT, *depth_hierarchy_tex.ref},
                                             {Trg::Tex2DSampled, COLOR_TEX_SLOT, *color_tex.ref},
                                             {Trg::Tex2DSampled, NORM_TEX_SLOT, *normal_tex.ref},
                                             {Trg::Tex2DSampled, NOISE_TEX_SLOT, *noise_tex.ref},
                                             {Trg::SBufRO, IN_RAY_LIST_SLOT, *in_ray_list_buf.ref},
                                             {Trg::Image, OUT_GI_IMG_SLOT, *out_gi_tex.ref},
                                             {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, *inout_ray_counter_buf.ref},
                                             {Trg::SBufRW, OUT_RAY_LIST_SLOT, *out_ray_list_buf.ref}};

            Params uniform_params;
            uniform_params.resolution = Ren::Vec4u(view_state_.act_res[0], view_state_.act_res[1], 0, 0);

            DispatchComputeIndirect(*pi_gi_trace_ss_, *indir_args_buf.ref, 0, bindings, &uniform_params,
                                    sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_struct_data.rt_tlas_buf && env_map) {
        FgResRef indir_rt_disp_buf;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("GI RT DISP ARGS");

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
                    rt_disp_args.AddStorageOutput("GI RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](FgBuilder &builder) {
                using namespace GIWriteIndirRTDispatch;

                FgAllocBuf &ray_counter_buf = builder.GetWriteBuffer(data->ray_counter);
                FgAllocBuf &indir_disp_buf = builder.GetWriteBuffer(data->indir_disp_buf);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, *ray_counter_buf.ref},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, *indir_disp_buf.ref}};

                DispatchCompute(*pi_gi_rt_write_indirect_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                                builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        { // Trace gi rays
            auto &rt_gi = fg_builder_.AddNode("RT GI");

            auto *data = rt_gi.AllocNodeData<ExRTGI::Args>();
            data->two_bounce = (settings.gi_quality == eGIQuality::Ultra);

            const auto stage = Stg::ComputeShader;

            data->geo_data = rt_gi.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_gi.AddStorageReadonlyInput(persistent_data.materials_buf, stage);
            data->vtx_buf1 = rt_gi.AddStorageReadonlyInput(persistent_data.vertex_buf1, stage);
            data->ndx_buf = rt_gi.AddStorageReadonlyInput(persistent_data.indices_buf, stage);
            data->shared_data = rt_gi.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->noise_tex = rt_gi.AddTextureInput(noise_tex, stage);
            data->depth_tex = rt_gi.AddTextureInput(frame_textures.depth, stage);
            data->normal_tex = rt_gi.AddTextureInput(frame_textures.normal, stage);
            data->env_tex = rt_gi.AddTextureInput(env_map, stage);
            data->ray_counter = rt_gi.AddStorageReadonlyInput(ray_counter, stage);
            data->ray_list = rt_gi.AddStorageReadonlyInput(ray_rt_list, stage);
            data->indir_args = rt_gi.AddIndirectBufferInput(indir_rt_disp_buf);
            data->tlas_buf = rt_gi.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, stage);
            data->lights_buf = rt_gi.AddStorageReadonlyInput(common_buffers.lights, stage);
            data->shadow_depth_tex = rt_gi.AddTextureInput(shadow_depth_tex_, stage);
            data->shadow_color_tex = rt_gi.AddTextureInput(shadow_color_tex_, stage);
            data->ltc_luts_tex = rt_gi.AddTextureInput(ltc_luts_, stage);
            data->cells_buf = rt_gi.AddStorageReadonlyInput(common_buffers.rt_cells, stage);
            data->items_buf = rt_gi.AddStorageReadonlyInput(common_buffers.rt_items, stage);

            if (persistent_data.stoch_lights_buf) {
                data->stoch_lights_buf =
                    rt_gi.AddStorageReadonlyInput(persistent_data.stoch_lights_buf, Stg::ComputeShader);
                data->light_nodes_buf =
                    rt_gi.AddStorageReadonlyInput(persistent_data.stoch_lights_nodes_buf, Stg::ComputeShader);
            }

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf = rt_gi.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx_buf =
                    rt_gi.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, stage);
                data->swrt.mesh_instances_buf = rt_gi.AddStorageReadonlyInput(rt_obj_instances_res, stage);
#if defined(REN_GL_BACKEND)
                data->swrt.textures_buf = rt_gi.AddStorageReadonlyInput(bindless.textures_buf, stage);
#endif
            }

            data->irradiance_tex = rt_gi.AddTextureInput(frame_textures.gi_cache_irradiance, stage);
            data->distance_tex = rt_gi.AddTextureInput(frame_textures.gi_cache_distance, stage);
            data->offset_tex = rt_gi.AddTextureInput(frame_textures.gi_cache_offset, stage);

            data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

            gi_tex = data->out_gi_tex = rt_gi.AddStorageImageOutput(gi_tex, stage);

            ex_rt_gi_.Setup(fg_builder_, &view_state_, &bindless, data);
            rt_gi.set_executor(&ex_rt_gi_);
        }

        { // Direct light sampling
            auto &sample_lights = fg_builder_.AddNode("SAMPLE LIGHTS");

            auto *data = sample_lights.AllocNodeData<ExSampleLights::Args>();
            data->shared_data = sample_lights.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->random_seq = sample_lights.AddStorageReadonlyInput(pmj_samples_buf_, Stg::ComputeShader);
            if (persistent_data.stoch_lights_buf) {
                data->lights_buf =
                    sample_lights.AddStorageReadonlyInput(persistent_data.stoch_lights_buf, Stg::ComputeShader);
                data->nodes_buf =
                    sample_lights.AddStorageReadonlyInput(persistent_data.stoch_lights_nodes_buf, Stg::ComputeShader);
            }

            data->geo_data = sample_lights.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
            data->materials = sample_lights.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::ComputeShader);
            data->vtx_buf1 = sample_lights.AddStorageReadonlyInput(persistent_data.vertex_buf1, Stg::ComputeShader);
            data->ndx_buf = sample_lights.AddStorageReadonlyInput(persistent_data.indices_buf, Stg::ComputeShader);
            data->tlas_buf = sample_lights.AddStorageReadonlyInput(acc_struct_data.rt_tlas_buf, Stg::ComputeShader);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = persistent_data.swrt.rt_root_node;
                data->swrt.rt_blas_buf =
                    sample_lights.AddStorageReadonlyInput(persistent_data.swrt.rt_blas_buf, Stg::ComputeShader);
                data->swrt.prim_ndx_buf =
                    sample_lights.AddStorageReadonlyInput(persistent_data.swrt.rt_prim_indices_buf, Stg::ComputeShader);
                data->swrt.mesh_instances_buf =
                    sample_lights.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
#if defined(REN_GL_BACKEND)
                data->swrt.textures_buf =
                    sample_lights.AddStorageReadonlyInput(bindless.textures_buf, Stg::ComputeShader);
#endif
            }

            data->tlas = acc_struct_data.rt_tlases[int(eTLASIndex::Main)];

            data->albedo_tex = sample_lights.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
            data->depth_tex = sample_lights.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->norm_tex = sample_lights.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->spec_tex = sample_lights.AddTextureInput(frame_textures.specular, Stg::ComputeShader);

            gi_tex = data->out_diffuse_tex = sample_lights.AddStorageImageOutput(gi_tex, Stg::ComputeShader);

            { // reflections texture
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
                data->out_specular_tex = sample_lights.AddStorageImageOutput("SSR Temp 2", params, Stg::ComputeShader);
            }

            ex_sample_lights_.Setup(fg_builder_, &view_state_, &bindless, data);
            sample_lights.set_executor(&ex_sample_lights_);
        }
    }

    if (debug_denoise) {
        frame_textures.gi = gi_tex;
        return;
    }

    FgResRef reproj_gi_tex, avg_gi_tex;
    FgResRef variance_temp_tex, sample_count_tex;

    { // Denoiser reprojection
        auto &gi_reproject = fg_builder_.AddNode("GI REPROJECT");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex, norm_tex, velocity_tex;
            FgResRef depth_hist_tex, norm_hist_tex, gi_hist_tex, variance_hist_tex, sample_count_hist_tex;
            FgResRef gi_tex;
            FgResRef tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_reprojected_tex, out_avg_gi_tex;
            FgResRef out_variance_tex, out_sample_count_tex;
        };

        auto *data = gi_reproject.AllocNodeData<PassData>();
        data->shared_data = gi_reproject.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth_tex = gi_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = gi_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity_tex = gi_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist_tex = gi_reproject.AddHistoryTextureInput(OPAQUE_DEPTH_TEX, Stg::ComputeShader);
        data->norm_hist_tex = gi_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->gi_hist_tex = gi_reproject.AddHistoryTextureInput("GI Diffuse Filtered", Stg::ComputeShader);
        data->variance_hist_tex = gi_reproject.AddHistoryTextureInput(DIFFUSE_VARIANCE_TEX, Stg::ComputeShader);
        gi_tex = data->gi_tex = gi_reproject.AddTextureInput(gi_tex, Stg::ComputeShader);

        data->tile_list = gi_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = gi_reproject.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Reprojected gi texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            reproj_gi_tex = data->out_reprojected_tex =
                gi_reproject.AddStorageImageOutput("GI Reprojected", params, Stg::ComputeShader);
        }
        { // 8x8 average gi texture
            Ren::TexParams params;
            params.w = (view_state_.scr_res[0] + 7) / 8;
            params.h = (view_state_.scr_res[1] + 7) / 8;
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            avg_gi_tex = data->out_avg_gi_tex =
                gi_reproject.AddStorageImageOutput("Average GI", params, Stg::ComputeShader);
        }
        { // Variance
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            variance_temp_tex = data->out_variance_tex =
                gi_reproject.AddStorageImageOutput("GI Variance Temp", params, Stg::ComputeShader);
        }
        { // Sample count
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sample_count_tex = data->out_sample_count_tex =
                gi_reproject.AddStorageImageOutput("GI Sample Count", params, Stg::ComputeShader);
        }

        data->sample_count_hist_tex =
            gi_reproject.AddHistoryTextureInput(data->out_sample_count_tex, Stg::ComputeShader);

        gi_reproject.set_execute_cb([this, data, tile_count](FgBuilder &builder) {
            FgAllocBuf &shared_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
            FgAllocTex &depth_hist_tex = builder.GetReadTexture(data->depth_hist_tex);
            FgAllocTex &norm_hist_tex = builder.GetReadTexture(data->norm_hist_tex);
            FgAllocTex &gi_hist_tex = builder.GetReadTexture(data->gi_hist_tex);
            FgAllocTex &variance_hist_tex = builder.GetReadTexture(data->variance_hist_tex);
            FgAllocTex &sample_count_hist_tex = builder.GetReadTexture(data->sample_count_hist_tex);
            FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);
            FgAllocTex &out_reprojected_tex = builder.GetWriteTexture(data->out_reprojected_tex);
            FgAllocTex &out_avg_gi_tex = builder.GetWriteTexture(data->out_avg_gi_tex);
            FgAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);
            FgAllocTex &out_sample_count_tex = builder.GetWriteTexture(data->out_sample_count_tex);

            { // Process tiles
                using namespace GIReproject;

                const Ren::Binding bindings[] = {
                    {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *shared_data_buf.ref},
                    {Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                    {Trg::Tex2DSampled, NORM_TEX_SLOT, *norm_tex.ref},
                    {Trg::Tex2DSampled, VELOCITY_TEX_SLOT, *velocity_tex.ref},
                    {Trg::Tex2DSampled, DEPTH_HIST_TEX_SLOT, {*depth_hist_tex.ref, 1}},
                    {Trg::Tex2DSampled, NORM_HIST_TEX_SLOT, *norm_hist_tex.ref},
                    {Trg::Tex2DSampled, GI_HIST_TEX_SLOT, *gi_hist_tex.ref},
                    {Trg::Tex2DSampled, VARIANCE_HIST_TEX_SLOT, *variance_hist_tex.ref},
                    {Trg::Tex2DSampled, SAMPLE_COUNT_HIST_TEX_SLOT, *sample_count_hist_tex.ref},
                    {Trg::Tex2DSampled, GI_TEX_SLOT, *gi_tex.ref},
                    {Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                    {Trg::Image, OUT_REPROJECTED_IMG_SLOT, *out_reprojected_tex.ref},
                    {Trg::Image, OUT_AVG_GI_IMG_SLOT, *out_avg_gi_tex.ref},
                    {Trg::Image, OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref},
                    {Trg::Image, OUT_SAMPLE_COUNT_IMG_SLOT, *out_sample_count_tex.ref}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
                uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

                DispatchComputeIndirect(*pi_gi_reproject_, *indir_args_buf.ref, data->indir_args_offset1, bindings,
                                        &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                        builder.ctx().log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                 {Trg::Image, OUT_RAD_IMG_SLOT, *out_reprojected_tex.ref},
                                                 {Trg::Image, OUT_AVG_RAD_IMG_SLOT, *out_avg_gi_tex.ref},
                                                 {Trg::Image, OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[3], *indir_args_buf.ref, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                        builder.ctx().log());
            }
        });
    }

    FgResRef prefiltered_gi;

    { // Denoiser prefilter
        auto &gi_prefilter = fg_builder_.AddNode("GI PREFILTER");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex, spec_tex, norm_tex, gi_tex, avg_gi_tex;
            FgResRef sample_count_tex, tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_gi_tex;
        };

        auto *data = gi_prefilter.AllocNodeData<PassData>();
        data->shared_data = gi_prefilter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->spec_tex = gi_prefilter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->depth_tex = gi_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm_tex = gi_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        gi_tex = data->gi_tex = gi_prefilter.AddTextureInput(gi_tex, Stg::ComputeShader);
        data->avg_gi_tex = gi_prefilter.AddTextureInput(avg_gi_tex, Stg::ComputeShader);
        data->sample_count_tex = gi_prefilter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->tile_list = gi_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = gi_prefilter.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Final diffuse
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            prefiltered_gi = data->out_gi_tex =
                gi_prefilter.AddStorageImageOutput("GI Diffuse 1", params, Stg::ComputeShader);
        }

        gi_prefilter.set_execute_cb([this, data, tile_count](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            FgAllocTex &avg_gi_tex = builder.GetReadTexture(data->avg_gi_tex);
            FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

            { // Filter tiles
                using namespace GIFilter;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                 {Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                 {Trg::Tex2DSampled, SPEC_TEX_SLOT, *spec_tex.ref},
                                                 {Trg::Tex2DSampled, NORM_TEX_SLOT, *norm_tex.ref},
                                                 {Trg::Tex2DSampled, GI_TEX_SLOT, {*gi_tex.ref, *nearest_sampler_}},
                                                 {Trg::Tex2DSampled, AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                                                 {Trg::Tex2DSampled, SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                 {Trg::Image, OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

                Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[0];
                uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                DispatchComputeIndirect(*pi_gi_filter_[settings.taa_mode == eTAAMode::Static], *indir_args_buf.ref,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        builder.ctx().default_descr_alloc(), builder.ctx().log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                 {Trg::Image, OUT_RAD_IMG_SLOT, *out_gi_tex.ref}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[0], *indir_args_buf.ref, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                        builder.ctx().log());
            }
        });
    }

    FgResRef gi_diffuse_tex, gi_variance_tex;

    { // Denoiser accumulation
        auto &gi_temporal = fg_builder_.AddNode("GI TEMPORAL");

        struct PassData {
            FgResRef shared_data;
            FgResRef norm_tex, avg_gi_tex, fallback_gi_tex, gi_tex, reproj_gi_tex;
            FgResRef variance_tex, sample_count_tex, tile_list;
            FgResRef indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgResRef out_gi_tex, out_variance_tex;
        };

        auto *data = gi_temporal.AllocNodeData<PassData>();
        data->shared_data = gi_temporal.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->norm_tex = gi_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_gi_tex = gi_temporal.AddTextureInput(avg_gi_tex, Stg::ComputeShader);
        data->fallback_gi_tex = gi_temporal.AddTextureInput(gi_fallback, Stg::ComputeShader);
        data->gi_tex = gi_temporal.AddTextureInput(prefiltered_gi, Stg::ComputeShader);
        data->reproj_gi_tex = gi_temporal.AddTextureInput(reproj_gi_tex, Stg::ComputeShader);
        data->variance_tex = gi_temporal.AddTextureInput(variance_temp_tex, Stg::ComputeShader);
        data->sample_count_tex = gi_temporal.AddTextureInput(sample_count_tex, Stg::ComputeShader);
        data->tile_list = gi_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = gi_temporal.AddIndirectBufferInput(indir_disp_buf);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        if (EnableBlur) {
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_diffuse_tex = data->out_gi_tex =
                gi_temporal.AddStorageImageOutput("GI Diffuse", params, Stg::ComputeShader);
        } else {
            gi_diffuse_tex = gi_tex = data->out_gi_tex = gi_temporal.AddStorageImageOutput(gi_tex, Stg::ComputeShader);
        }

        { // Variance texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gi_variance_tex = data->out_variance_tex =
                gi_temporal.AddStorageImageOutput(DIFFUSE_VARIANCE_TEX, params, Stg::ComputeShader);
        }

        gi_temporal.set_execute_cb([this, data, tile_count](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
            FgAllocTex &avg_gi_tex = builder.GetReadTexture(data->avg_gi_tex);
            FgAllocTex &fallback_gi_tex = builder.GetReadTexture(data->fallback_gi_tex);
            FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
            FgAllocTex &reproj_gi_tex = builder.GetReadTexture(data->reproj_gi_tex);
            FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
            FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
            FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
            FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

            FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);
            FgAllocTex &out_variance_tex = builder.GetWriteTexture(data->out_variance_tex);

            { // Process tiles
                using namespace GIResolveTemporal;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                 {Trg::Tex2DSampled, NORM_TEX_SLOT, *norm_tex.ref},
                                                 {Trg::Tex2DSampled, AVG_GI_TEX_SLOT, *avg_gi_tex.ref},
                                                 {Trg::Tex2DSampled, FALLBACK_TEX_SLOT, *fallback_gi_tex.ref},
                                                 {Trg::Tex2DSampled, GI_TEX_SLOT, *gi_tex.ref},
                                                 {Trg::Tex2DSampled, REPROJ_GI_TEX_SLOT, *reproj_gi_tex.ref},
                                                 {Trg::Tex2DSampled, VARIANCE_TEX_SLOT, *variance_tex.ref},
                                                 {Trg::Tex2DSampled, SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                 {Trg::Image, OUT_GI_IMG_SLOT, *out_gi_tex.ref},
                                                 {Trg::Image, OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);

                DispatchComputeIndirect(*pi_gi_temporal_[settings.taa_mode == eTAAMode::Static], *indir_args_buf.ref,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        builder.ctx().default_descr_alloc(), builder.ctx().log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                 {Trg::Image, OUT_RAD_IMG_SLOT, *out_gi_tex.ref},
                                                 {Trg::Image, OUT_VARIANCE_IMG_SLOT, *out_variance_tex.ref}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(*pi_tile_clear_[2], *indir_args_buf.ref, data->indir_args_offset2, bindings,
                                        &uniform_params, sizeof(uniform_params), builder.ctx().default_descr_alloc(),
                                        builder.ctx().log());
            }
        });
    }

    if (EnableBlur) {
        FgResRef gi_diffuse2_tex;

        { // Denoiser blur
            auto &gi_filter = fg_builder_.AddNode("GI FILTER");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, gi_tex;
                FgResRef sample_count_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgResRef out_gi_tex;
            };

            auto *data = gi_filter.AllocNodeData<PassData>();
            data->shared_data = gi_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->spec_tex = gi_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->depth_tex = gi_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->norm_tex = gi_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi_tex = gi_filter.AddTextureInput(gi_diffuse_tex, Stg::ComputeShader);
            data->sample_count_tex = gi_filter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->tile_list = gi_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = gi_filter.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final diffuse
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                gi_diffuse2_tex = data->out_gi_tex =
                    gi_filter.AddStorageImageOutput("GI Diffuse 2", params, Stg::ComputeShader);
            }

            gi_filter.set_execute_cb([this, data, tile_count](FgBuilder &builder) {
                FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
                FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
                FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
                // FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
                FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
                FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

                { // Filter tiles
                    using namespace GIFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                     {Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                     {Trg::Tex2DSampled, SPEC_TEX_SLOT, *spec_tex.ref},
                                                     {Trg::Tex2DSampled, NORM_TEX_SLOT, *norm_tex.ref},
                                                     {Trg::Tex2DSampled, GI_TEX_SLOT, {*gi_tex.ref, *nearest_sampler_}},
                                                     {Trg::Tex2DSampled, SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                                                     //{Trg::Tex2DSampled, VARIANCE_TEX_SLOT, *variance_tex.ref},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                     {Trg::Image, OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[0];
                    uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(*pi_gi_filter_[2], *indir_args_buf.ref, data->indir_args_offset1, bindings,
                                            &uniform_params, sizeof(uniform_params),
                                            builder.ctx().default_descr_alloc(), builder.ctx().log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                     {Trg::Image, OUT_RAD_IMG_SLOT, *out_gi_tex.ref}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(*pi_tile_clear_[0], *indir_args_buf.ref, data->indir_args_offset2, bindings,
                                            &uniform_params, sizeof(uniform_params),
                                            builder.ctx().default_descr_alloc(), builder.ctx().log());
                }
            });
        }

        FgResRef gi_diffuse3_tex;

        { // Denoiser blur 2
            auto &gi_post_filter = fg_builder_.AddNode("GI POST FILTER");

            struct PassData {
                FgResRef shared_data;
                FgResRef depth_tex, spec_tex, norm_tex, gi_tex, raylen_tex;
                FgResRef sample_count_tex, tile_list;
                FgResRef indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgResRef out_gi_tex;
            };

            auto *data = gi_post_filter.AllocNodeData<PassData>();
            data->shared_data = gi_post_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth_tex = gi_post_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec_tex = gi_post_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm_tex = gi_post_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi_tex = gi_post_filter.AddTextureInput(gi_diffuse2_tex, Stg::ComputeShader);
            data->sample_count_tex = gi_post_filter.AddTextureInput(sample_count_tex, Stg::ComputeShader);
            data->tile_list = gi_post_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = gi_post_filter.AddIndirectBufferInput(indir_disp_buf);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final diffuse
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                gi_diffuse3_tex = data->out_gi_tex =
                    gi_post_filter.AddStorageImageOutput("GI Diffuse Filtered", params, Stg::ComputeShader);
            }

            gi_post_filter.set_execute_cb([this, data, tile_count](FgBuilder &builder) {
                FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);
                FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
                FgAllocTex &sample_count_tex = builder.GetReadTexture(data->sample_count_tex);
                // FgAllocTex &variance_tex = builder.GetReadTexture(data->variance_tex);
                FgAllocBuf &tile_list_buf = builder.GetReadBuffer(data->tile_list);
                FgAllocBuf &indir_args_buf = builder.GetReadBuffer(data->indir_args);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

                { // Filter tiles
                    using namespace GIFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                                     {Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                     {Trg::Tex2DSampled, SPEC_TEX_SLOT, *spec_tex.ref},
                                                     {Trg::Tex2DSampled, NORM_TEX_SLOT, *norm_tex.ref},
                                                     {Trg::Tex2DSampled, GI_TEX_SLOT, {*gi_tex.ref, *nearest_sampler_}},
                                                     {Trg::Tex2DSampled, SAMPLE_COUNT_TEX_SLOT, *sample_count_tex.ref},
                                                     //{Trg::Tex2DSampled, VARIANCE_TEX_SLOT, *variance_tex.ref},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                     {Trg::Image, OUT_DENOISED_IMG_SLOT, *out_gi_tex.ref}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[1];
                    uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(*pi_gi_filter_[3], *indir_args_buf.ref, data->indir_args_offset1, bindings,
                                            &uniform_params, sizeof(uniform_params),
                                            builder.ctx().default_descr_alloc(), builder.ctx().log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, *tile_list_buf.ref},
                                                     {Trg::Image, OUT_RAD_IMG_SLOT, *out_gi_tex.ref}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(*pi_tile_clear_[0], *indir_args_buf.ref, data->indir_args_offset2, bindings,
                                            &uniform_params, sizeof(uniform_params),
                                            builder.ctx().default_descr_alloc(), builder.ctx().log());
                }
            });
        }

        FgResRef gi_diffuse4_tex;

        { // Denoiser stabilization
            auto &gi_stabilization = fg_builder_.AddNode("GI STABILIZATION");

            struct PassData {
                FgResRef depth_tex, velocity_tex, gi_tex, gi_hist_tex;
                FgResRef out_gi_tex;
            };

            auto *data = gi_stabilization.AllocNodeData<PassData>();
            data->depth_tex = gi_stabilization.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->velocity_tex = gi_stabilization.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
            data->gi_tex = gi_stabilization.AddTextureInput(gi_diffuse3_tex, Stg::ComputeShader);

            { // Final gi
                Ren::TexParams params;
                params.w = view_state_.scr_res[0];
                params.h = view_state_.scr_res[1];
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                gi_diffuse4_tex = data->out_gi_tex =
                    gi_stabilization.AddStorageImageOutput("GI Diffuse 4", params, Stg::ComputeShader);
            }

            data->gi_hist_tex = gi_stabilization.AddHistoryTextureInput(gi_diffuse4_tex, Stg::ComputeShader);

            gi_stabilization.set_execute_cb([this, data](FgBuilder &builder) {
                using namespace GIStabilization;

                FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
                FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
                FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
                FgAllocTex &gi_hist_tex = builder.GetReadTexture(data->gi_hist_tex);

                FgAllocTex &out_gi_tex = builder.GetWriteTexture(data->out_gi_tex);

                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                 {Trg::Tex2DSampled, VELOCITY_TEX_SLOT, *velocity_tex.ref},
                                                 {Trg::Tex2DSampled, GI_TEX_SLOT, *gi_tex.ref},
                                                 {Trg::Tex2DSampled, GI_HIST_TEX_SLOT, *gi_hist_tex.ref},
                                                 {Trg::Image, OUT_GI_IMG_SLOT, *out_gi_tex.ref}};

                const Ren::Vec3u grp_count =
                    Ren::Vec3u{(view_state_.act_res[0] + LOCAL_GROUP_SIZE_X - 1u) / LOCAL_GROUP_SIZE_X,
                               (view_state_.act_res[1] + LOCAL_GROUP_SIZE_Y - 1u) / LOCAL_GROUP_SIZE_Y, 1u};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);

                DispatchCompute(*pi_gi_stabilization_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                                builder.ctx().default_descr_alloc(), builder.ctx().log());
            });
        }

        if (EnableStabilization) {
            frame_textures.gi = gi_diffuse4_tex;
        } else {
            frame_textures.gi = gi_diffuse3_tex;
        }
    } else {
        frame_textures.gi = gi_diffuse_tex;
    }
}

void Eng::Renderer::AddSSAOPasses(const FgResRef depth_down_2x, const FgResRef _depth_tex, FgResRef &out_ssao) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    FgResRef ssao_raw;
    { // Main SSAO pass
        auto &ssao = fg_builder_.AddNode("SSAO");

        struct PassData {
            FgResRef rand_tex;
            FgResRef depth_tex;

            FgResRef output_tex;
        };

        auto *data = ssao.AllocNodeData<PassData>();
        data->rand_tex = ssao.AddTextureInput(rand2d_dirs_4x4_, Stg::FragmentShader);
        data->depth_tex = ssao.AddTextureInput(depth_down_2x, Stg::FragmentShader);

        { // Allocate output texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_raw = data->output_tex = ssao.AddColorOutput("SSAO RAW", params);
        }

        ssao.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &down_depth_2x_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &rand_tex = builder.GetReadTexture(data->rand_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.act_res[0] / 2;
            rast_state.viewport[3] = view_state_.act_res[1] / 2;

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, SSAO::DEPTH_TEX_SLOT, *down_depth_2x_tex.ref},
                                             {Trg::Tex2DSampled, SSAO::RAND_TEX_SLOT, *rand_tex.ref}};

            SSAO::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_.act_res[0] / 2, view_state_.act_res[1] / 2};
            uniform_params.resolution = Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

            const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

            prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ao_prog_, {}, render_targets, rast_state,
                                builder.rast_state(), bindings, &uniform_params, sizeof(SSAO::Params), 0);
        });
    }

    FgResRef ssao_blurred1;
    { // Horizontal SSAO blur
        auto &ssao_blur_h = fg_builder_.AddNode("SSAO BLUR H");

        struct PassData {
            FgResRef depth_tex;
            FgResRef input_tex;

            FgResRef output_tex;
        };

        auto *data = ssao_blur_h.AllocNodeData<PassData>();
        data->depth_tex = ssao_blur_h.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->input_tex = ssao_blur_h.AddTextureInput(ssao_raw, Stg::FragmentShader);

        { // Allocate output texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_blurred1 = data->output_tex = ssao_blur_h.AddColorOutput("SSAO BLUR TEMP1", params);
        }

        ssao_blur_h.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.act_res[0] / 2;
            rast_state.viewport[3] = view_state_.act_res[1] / 2;

            { // blur ao buffer
                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, Bilateral::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                 {Trg::Tex2DSampled, Bilateral::INPUT_TEX_SLOT, *input_tex.ref}};

                Bilateral::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution = Ren::Vec2f{float(rast_state.viewport[2]), float(rast_state.viewport[3])};
                uniform_params.vertical = 0.0f;

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, {}, render_targets, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params), 0);
            }
        });
    }

    FgResRef ssao_blurred2;
    { // Vertical SSAO blur
        auto &ssao_blur_v = fg_builder_.AddNode("SSAO BLUR V");

        struct PassData {
            FgResRef depth_tex;
            FgResRef input_tex;

            FgResRef output_tex;
        };

        auto *data = ssao_blur_v.AllocNodeData<PassData>();
        data->depth_tex = ssao_blur_v.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->input_tex = ssao_blur_v.AddTextureInput(ssao_blurred1, Stg::FragmentShader);

        { // Allocate output texture
            Ren::TexParams params;
            params.w = view_state_.scr_res[0] / 2;
            params.h = view_state_.scr_res[1] / 2;
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            ssao_blurred2 = data->output_tex = ssao_blur_v.AddColorOutput("SSAO BLUR TEMP2", params);
        }

        ssao_blur_v.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.act_res[0] / 2;
            rast_state.viewport[3] = view_state_.act_res[1] / 2;

            { // blur ao buffer
                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, Bilateral::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                                 {Trg::Tex2DSampled, Bilateral::INPUT_TEX_SLOT, *input_tex.ref}};

                Bilateral::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution = Ren::Vec2f{float(rast_state.viewport[2]), float(rast_state.viewport[3])};
                uniform_params.vertical = 1.0f;

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_bilateral_prog_, {}, render_targets, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params), 0);
            }
        });
    }

    { // Upscale SSAO pass
        auto &ssao_upscale = fg_builder_.AddNode("UPSCALE");

        struct PassData {
            FgResRef depth_down_2x_tex;
            FgResRef depth_tex;
            FgResRef input_tex;

            FgResRef output_tex;
        };

        auto *data = ssao_upscale.AllocNodeData<PassData>();
        data->depth_down_2x_tex = ssao_upscale.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->depth_tex = ssao_upscale.AddTextureInput(_depth_tex, Stg::FragmentShader);
        data->input_tex = ssao_upscale.AddTextureInput(ssao_blurred2, Stg::FragmentShader);

        { // Allocate output texture
            Ren::TexParams params;
            params.w = view_state_.act_res[0];
            params.h = view_state_.act_res[1];
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            out_ssao = data->output_tex = ssao_upscale.AddColorOutput("SSAO Final", params);
        }

        ssao_upscale.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &down_depth_2x_tex = builder.GetReadTexture(data->depth_down_2x_tex);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.viewport[2] = view_state_.act_res[0];
            rast_state.viewport[3] = view_state_.act_res[1];
            { // upsample ao
                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
                const Ren::Binding bindings[] = {
                    {Trg::Tex2DSampled, Upscale::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                    {Trg::Tex2DSampled, Upscale::DEPTH_LOW_TEX_SLOT, *down_depth_2x_tex.ref},
                    {Trg::Tex2DSampled, Upscale::INPUT_TEX_SLOT, *input_tex.ref}};
                Upscale::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution = Ren::Vec4f{float(view_state_.act_res[0]), float(view_state_.act_res[1]),
                                                       float(view_state_.scr_res[0]), float(view_state_.scr_res[1])};
                uniform_params.clip_info = view_state_.clip_info;
                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_upscale_prog_, {}, render_targets, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(Upscale::Params), 0);
            }
        });
    }
}

Eng::FgResRef Eng::Renderer::AddGTAOPasses(const eSSAOQuality quality, FgResRef depth_tex, FgResRef velocity_tex,
                                           FgResRef norm_tex) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    FgResRef gtao_result;
    { // main pass
        auto &gtao_main = fg_builder_.AddNode("GTAO MAIN");

        struct PassData {
            FgResRef depth_tex;
            FgResRef norm_tex;
            FgResRef output_tex;
        };

        auto *data = gtao_main.AllocNodeData<PassData>();
        data->depth_tex = gtao_main.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->norm_tex = gtao_main.AddTextureInput(norm_tex, Stg::ComputeShader);

        { // Output texture
            Ren::TexParams params;
            params.w = quality == eSSAOQuality::Ultra ? view_state_.scr_res[0] : (view_state_.scr_res[0] / 2);
            params.h = quality == eSSAOQuality::Ultra ? view_state_.scr_res[1] : (view_state_.scr_res[1] / 2);
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gtao_result = data->output_tex = gtao_main.AddStorageImageOutput("GTAO RAW", params, Stg::ComputeShader);
        }

        gtao_main.set_execute_cb([this, data, quality](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &norm_tex = builder.GetReadTexture(data->norm_tex);

            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, GTAO::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, GTAO::NORM_TEX_SLOT, *norm_tex.ref},
                                             {Trg::Image, GTAO::OUT_IMG_SLOT, *output_tex.ref}};

            const uint32_t img_size[2] = {quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[0])
                                                                         : uint32_t(view_state_.act_res[0] / 2),
                                          quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[1])
                                                                         : uint32_t(view_state_.act_res[1] / 2)};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(img_size[0] + GTAO::LOCAL_GROUP_SIZE_X - 1u) / GTAO::LOCAL_GROUP_SIZE_X,
                           (img_size[1] + GTAO::LOCAL_GROUP_SIZE_Y - 1u) / GTAO::LOCAL_GROUP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{img_size[0], img_size[1]};
            uniform_params.rand[0] = RendererInternal::GTAORandSamples[view_state_.frame_index % 32][0];
            uniform_params.rand[1] = RendererInternal::GTAORandSamples[view_state_.frame_index % 32][1];
            uniform_params.clip_info = view_state_.clip_info;
            uniform_params.frustum_info = view_state_.frustum_info;
            uniform_params.view_from_world = view_state_.view_from_world;

            DispatchCompute(*pi_gtao_main_[quality == eSSAOQuality::High], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    { // filter pass
        auto &gtao_filter = fg_builder_.AddNode("GTAO FILTER");

        struct PassData {
            FgResRef depth_tex;
            FgResRef ao_tex;
            FgResRef out_ao_tex;
        };

        auto *data = gtao_filter.AllocNodeData<PassData>();
        data->depth_tex = gtao_filter.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->ao_tex = gtao_filter.AddTextureInput(gtao_result, Stg::ComputeShader);

        { // Output texture
            Ren::TexParams params;
            params.w = quality == eSSAOQuality::Ultra ? view_state_.scr_res[0] : (view_state_.scr_res[0] / 2);
            params.h = quality == eSSAOQuality::Ultra ? view_state_.scr_res[1] : (view_state_.scr_res[1] / 2);
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gtao_result = data->out_ao_tex =
                gtao_filter.AddStorageImageOutput("GTAO FILTERED", params, Stg::ComputeShader);
        }

        gtao_filter.set_execute_cb([this, data, quality](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &ao_tex = builder.GetReadTexture(data->ao_tex);

            FgAllocTex &out_ao_tex = builder.GetWriteTexture(data->out_ao_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, GTAO::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, GTAO::GTAO_TEX_SLOT, *ao_tex.ref},
                                             {Trg::Image, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

            const uint32_t img_size[2] = {quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[0])
                                                                         : uint32_t(view_state_.act_res[0] / 2),
                                          quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[1])
                                                                         : uint32_t(view_state_.act_res[1] / 2)};

            const auto grp_count =
                Ren::Vec3u{(img_size[0] + GTAO::LOCAL_GROUP_SIZE_X - 1u) / GTAO::LOCAL_GROUP_SIZE_X,
                           (img_size[1] + GTAO::LOCAL_GROUP_SIZE_Y - 1u) / GTAO::LOCAL_GROUP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{img_size[0], img_size[1]};
            uniform_params.clip_info = view_state_.clip_info;

            DispatchCompute(*pi_gtao_filter_[quality == eSSAOQuality::High], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    { // accumulation pass
        auto &gtao_accumulation = fg_builder_.AddNode("GTAO ACCUMULATE");

        struct PassData {
            FgResRef depth_tex, depth_hist_tex, velocity_tex, ao_tex, ao_hist_tex;
            FgResRef out_ao_tex;
        };

        auto *data = gtao_accumulation.AllocNodeData<PassData>();
        data->depth_tex = gtao_accumulation.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->depth_hist_tex = gtao_accumulation.AddHistoryTextureInput(depth_tex, Stg::ComputeShader);
        data->velocity_tex = gtao_accumulation.AddTextureInput(velocity_tex, Stg::ComputeShader);
        data->ao_tex = gtao_accumulation.AddTextureInput(gtao_result, Stg::ComputeShader);

        { // Final ao
            Ren::TexParams params;
            params.w = quality == eSSAOQuality::Ultra ? view_state_.scr_res[0] : (view_state_.scr_res[0] / 2);
            params.h = quality == eSSAOQuality::Ultra ? view_state_.scr_res[1] : (view_state_.scr_res[1] / 2);
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gtao_result = data->out_ao_tex =
                gtao_accumulation.AddStorageImageOutput("GTAO PRE FINAL", params, Stg::ComputeShader);
        }

        data->ao_hist_tex = gtao_accumulation.AddHistoryTextureInput(gtao_result, Stg::ComputeShader);

        gtao_accumulation.set_execute_cb([this, data, quality](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &depth_hist_tex = builder.GetReadTexture(data->depth_hist_tex);
            FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
            FgAllocTex &ao_tex = builder.GetReadTexture(data->ao_tex);
            FgAllocTex &ao_hist_tex = builder.GetReadTexture(data->ao_hist_tex);

            FgAllocTex &out_ao_tex = builder.GetWriteTexture(data->out_ao_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, GTAO::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, GTAO::DEPTH_HIST_TEX_SLOT, {*depth_hist_tex.ref, 1}},
                                             {Trg::Tex2DSampled, GTAO::VELOCITY_TEX_SLOT, *velocity_tex.ref},
                                             {Trg::Tex2DSampled, GTAO::GTAO_TEX_SLOT, *ao_tex.ref},
                                             {Trg::Tex2DSampled, GTAO::GTAO_HIST_TEX_SLOT, *ao_hist_tex.ref},
                                             {Trg::Image, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

            const uint32_t img_size[2] = {quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[0])
                                                                         : uint32_t(view_state_.act_res[0] / 2),
                                          quality == eSSAOQuality::Ultra ? uint32_t(view_state_.act_res[1])
                                                                         : uint32_t(view_state_.act_res[1] / 2)};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(img_size[0] + GTAO::LOCAL_GROUP_SIZE_X - 1u) / GTAO::LOCAL_GROUP_SIZE_X,
                           (img_size[1] + GTAO::LOCAL_GROUP_SIZE_Y - 1u) / GTAO::LOCAL_GROUP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{img_size[0], img_size[1]};
            uniform_params.clip_info = view_state_.clip_info;

            DispatchCompute(*pi_gtao_accumulate_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    if (quality != eSSAOQuality::Ultra) {
        auto &gtao_upsample = fg_builder_.AddNode("GTAO UPSAMPLE");

        struct PassData {
            FgResRef depth_tex, ao_tex;
            FgResRef out_ao_tex;
        };

        auto *data = gtao_upsample.AllocNodeData<PassData>();
        data->depth_tex = gtao_upsample.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->ao_tex = gtao_upsample.AddTextureInput(gtao_result, Stg::ComputeShader);

        { // Final ao
            Ren::TexParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::R8;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            gtao_result = data->out_ao_tex =
                gtao_upsample.AddStorageImageOutput("GTAO FINAL", params, Stg::ComputeShader);
        }

        gtao_upsample.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &ao_tex = builder.GetReadTexture(data->ao_tex);

            FgAllocTex &out_ao_tex = builder.GetWriteTexture(data->out_ao_tex);

            const Ren::Binding bindings[] = {{Trg::Tex2DSampled, GTAO::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, GTAO::GTAO_TEX_SLOT, *ao_tex.ref},
                                             {Trg::Image, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.act_res[0] + GTAO::LOCAL_GROUP_SIZE_X - 1u) / GTAO::LOCAL_GROUP_SIZE_X,
                           (view_state_.act_res[1] + GTAO::LOCAL_GROUP_SIZE_Y - 1u) / GTAO::LOCAL_GROUP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);

            DispatchCompute(*pi_gtao_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    return gtao_result;
}