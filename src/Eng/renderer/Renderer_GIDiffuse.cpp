#include "Renderer.h"

#include <Ren/Context.h>

#include "Renderer_Names.h"
#include "executors/ExRTDiffuse.h"
#include "executors/ExSampleLights.h"

#include "shaders/blit_bilateral_interface.h"
#include "shaders/blit_ssao_interface.h"
#include "shaders/blit_upscale_interface.h"
#include "shaders/gtao_interface.h"
#include "shaders/probe_sample_interface.h"
#include "shaders/rt_diffuse_classify_interface.h"
#include "shaders/rt_diffuse_filter_interface.h"
#include "shaders/rt_diffuse_interface.h"
#include "shaders/rt_diffuse_reproject_interface.h"
#include "shaders/rt_diffuse_stabilization_interface.h"
#include "shaders/rt_diffuse_temporal_interface.h"
#include "shaders/rt_diffuse_trace_ss_interface.h"
#include "shaders/rt_diffuse_write_indirect_args_interface.h"
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

void Eng::Renderer::AddDiffusePasses(const bool debug_denoise, const CommonBuffers &common_buffers,
                                     const AccelerationStructures &acc_structs, const BindlessTextureData &bindless,
                                     const FgImgROHandle depth_hierarchy, const FgBufROHandle rt_geo_instances_res,
                                     const FgBufROHandle rt_obj_instances_res, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgImgROHandle gi_fallback;

    if (frame_textures.gi_cache_irradiance) {
        auto &probe_sample = fg_builder_.AddNode("PROBE SAMPLE");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth;
            FgImgROHandle normals;
            FgImgROHandle ssao;
            FgImgROHandle irradiance;
            FgImgROHandle distance;
            FgImgROHandle offset;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = probe_sample.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth = probe_sample.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->normals = probe_sample.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->ssao = probe_sample.AddTextureInput(frame_textures.ssao, Stg::ComputeShader);
        data->irradiance = probe_sample.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
        data->distance = probe_sample.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
        data->offset = probe_sample.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

        { // gi texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gi_fallback = data->output = probe_sample.AddStorageImageOutput("GI Tex", desc, Stg::ComputeShader);
        }

        probe_sample.set_execute_cb([data, this](const FgContext &fg) {
            using namespace ProbeSample;

            const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle normals = fg.AccessROImage(data->normals);
            const Ren::ImageROHandle ssao = fg.AccessROImage(data->ssao);
            const Ren::ImageROHandle irr = fg.AccessROImage(data->irradiance);
            const Ren::ImageROHandle dist = fg.AccessROImage(data->distance);
            const Ren::ImageROHandle off = fg.AccessROImage(data->offset);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                             {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, NORM_TEX_SLOT, normals},
                                             {Trg::TexSampled, SSAO_TEX_SLOT, ssao},
                                             {Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr},
                                             {Trg::TexSampled, DISTANCE_TEX_SLOT, dist},
                                             {Trg::TexSampled, OFFSET_TEX_SLOT, off},
                                             {Trg::ImageRW, OUT_IMG_SLOT, output}};

            const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                    (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

            DispatchCompute(fg.cmd_buf(), pi_probe_sample_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    } else {
        gi_fallback = frame_textures.dummy_black;
    }

    if (settings.gi_quality <= eGIQuality::Medium) {
        frame_textures.gi_diffuse = gi_fallback;
        return;
    }

    // GI settings
    const uint32_t SamplesPerQuad = (settings.gi_quality == eGIQuality::Ultra) ? 4 : 1;
    static const bool VarianceGuided = true;
    static const bool EnableBlur = true;
    const bool EnableStabilization = false;
    //(settings.taa_mode != eTAAMode::Static);

    FgBufRWHandle ray_counter;

    { // Prepare atomic counter and ray length texture
        auto &gi_prepare = fg_builder_.AddNode("GI PREPARE");

        struct PassData {
            FgBufRWHandle ray_counter;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();

        { // ray counter
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = 16 * sizeof(uint32_t);

            ray_counter = data->ray_counter = gi_prepare.AddTransferOutput("GI Ray Counter", desc);
        }

        gi_prepare.set_execute_cb([data](const FgContext &fg) {
            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);

            const auto &[buf_main, buf_cold] = fg.storages().buffers[ray_counter];
            Buffer_Fill(fg.ren_ctx().api(), buf_main, 0, buf_cold.size, 0, fg.cmd_buf());
        });
    }

    const int tile_count = ((view_state_.ren_res[0] + 7) / 8) * ((view_state_.ren_res[1] + 7) / 8);

    FgBufRWHandle ray_list, tile_list;
    FgImgRWHandle gi_img, noise;

    { // Classify pixel quads
        auto &diff_classify = fg_builder_.AddNode("DIFF CLASSIFY");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle spec;
            FgImgROHandle variance_history;
            FgBufROHandle bn_pmj_seq;
            FgBufRWHandle ray_counter;
            FgBufRWHandle ray_list;
            FgBufRWHandle tile_list;
            FgImgRWHandle out_gi, out_noise;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = diff_classify.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->spec = diff_classify.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        if (debug_denoise) {
            data->variance_history = diff_classify.AddTextureInput(frame_textures.dummy_black, Stg::ComputeShader);
        } else {
            data->variance_history = diff_classify.AddHistoryTextureInput(DIFFUSE_VARIANCE_TEX, Stg::ComputeShader);
        }
        data->bn_pmj_seq =
            diff_classify.AddStorageReadonlyInput(common_buffers.bn_pmj_2D_64spp_seq, Stg::ComputeShader);
        ray_counter = data->ray_counter = diff_classify.AddStorageOutput(ray_counter, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_list = data->ray_list = diff_classify.AddStorageOutput("GI Ray List", desc, Stg::ComputeShader);
        }
        { // tile list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = tile_count * sizeof(uint32_t);

            tile_list = data->tile_list = diff_classify.AddStorageOutput("GI Tile List", desc, Stg::ComputeShader);
        }
        { // final gi texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;
            gi_img = data->out_gi = diff_classify.AddStorageImageOutput("GI Final", desc, Stg::ComputeShader);
        }
        { // blue noise texture
            FgImgDesc desc;
            desc.w = desc.h = 128;
            desc.format = Ren::eFormat::RGBA8;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::Repeat;
            noise = data->out_noise = diff_classify.AddStorageImageOutput("GI BN Tex", desc, Stg::ComputeShader);
        }

        diff_classify.set_execute_cb([this, data, tile_count, SamplesPerQuad](const FgContext &fg) {
            using namespace RTDiffuseClassifyTiles;

            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
            const Ren::ImageROHandle variance = fg.AccessROImage(data->variance_history);
            const Ren::BufferROHandle bn_pmj_seq = fg.AccessROBuffer(data->bn_pmj_seq);

            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle ray_list = fg.AccessRWBuffer(data->ray_list);
            const Ren::BufferHandle tile_list = fg.AccessRWBuffer(data->tile_list);

            const Ren::ImageRWHandle gi = fg.AccessRWImage(data->out_gi);
            const Ren::ImageRWHandle noise = fg.AccessRWImage(data->out_noise);

            const Ren::Binding bindings[] = {
                {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},  {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                {Trg::TexSampled, VARIANCE_TEX_SLOT, variance}, {Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                {Trg::SBufRW, RAY_LIST_SLOT, ray_list},         {Trg::SBufRW, TILE_LIST_SLOT, tile_list},
                {Trg::UTBuf, BN_PMJ_SEQ_BUF_SLOT, bn_pmj_seq},  {Trg::ImageRW, OUT_GI_IMG_SLOT, gi},
                {Trg::ImageRW, OUT_NOISE_IMG_SLOT, noise}};

            const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                    (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.samples_and_guided = Ren::Vec2u{SamplesPerQuad, VarianceGuided ? 1u : 0u};
            uniform_params.frame_index = view_state_.frame_index;
            uniform_params.tile_count = tile_count;

            DispatchCompute(fg.cmd_buf(), pi_diffuse_classify_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    FgBufRWHandle indir_disp;

    { // Write indirect arguments
        auto &write_indir = fg_builder_.AddNode("DIFF INDIR ARGS");

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

            indir_disp = data->indir_disp = write_indir.AddStorageOutput("GI Intersect Args", desc, Stg::ComputeShader);
        }

        write_indir.set_execute_cb([this, data](const FgContext &fg) {
            using namespace RTDiffuseWriteIndirectArgs;

            const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
            const Ren::BufferHandle indir_args = fg.AccessRWBuffer(data->indir_disp);

            const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                             {Trg::SBufRW, INDIR_ARGS_SLOT, indir_args}};

            DispatchCompute(fg.cmd_buf(), pi_diffuse_write_indirect_[0], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                            bindings, nullptr, 0, fg.descr_alloc(), fg.log());
        });
    }

    FgBufRWHandle ray_rt_list;

    { // Trace screen-space rays
        auto &diff_trace_ss = fg_builder_.AddNode("DIFF TRACE SS");

        struct PassData {
            FgImgROHandle noise;
            FgBufROHandle shared_data;
            FgImgROHandle color, normal;
            FgImgROHandle depth_hierarchy;

            FgBufROHandle in_ray_list, indir_args;
            FgBufRWHandle inout_ray_counter;
            FgImgRWHandle out_gi;
            FgBufRWHandle out_ray_list;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->noise = diff_trace_ss.AddTextureInput(noise, Stg::ComputeShader);
        data->shared_data = diff_trace_ss.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->color = diff_trace_ss.AddHistoryTextureInput(MAIN_COLOR_TEX, Stg::ComputeShader);
        data->normal = diff_trace_ss.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->depth_hierarchy = diff_trace_ss.AddTextureInput(depth_hierarchy, Stg::ComputeShader);

        data->in_ray_list = diff_trace_ss.AddStorageReadonlyInput(ray_list, Stg::ComputeShader);
        data->indir_args = diff_trace_ss.AddIndirectBufferInput(indir_disp);
        ray_counter = data->inout_ray_counter = diff_trace_ss.AddStorageOutput(ray_counter, Stg::ComputeShader);
        gi_img = data->out_gi = diff_trace_ss.AddStorageImageOutput(gi_img, Stg::ComputeShader);

        { // packed ray list
            FgBufDesc desc = {};
            desc.type = Ren::eBufType::Storage;
            desc.size = view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

            ray_rt_list = data->out_ray_list =
                diff_trace_ss.AddStorageOutput("GI RT Ray List", desc, Stg::ComputeShader);
        }

        diff_trace_ss.set_execute_cb([this, data](const FgContext &fg) {
            using namespace RTDiffuseTraceSS;

            const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle color = fg.AccessROImage(data->color);
            const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
            const Ren::ImageROHandle depth_hierarchy = fg.AccessROImage(data->depth_hierarchy);
            const Ren::BufferROHandle in_ray_list = fg.AccessROBuffer(data->in_ray_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);
            const Ren::BufferRWHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);
            const Ren::BufferRWHandle out_ray_list = fg.AccessRWBuffer(data->out_ray_list);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                             {Trg::TexSampled, DEPTH_TEX_SLOT, depth_hierarchy},
                                             {Trg::TexSampled, COLOR_TEX_SLOT, color},
                                             {Trg::TexSampled, NORM_TEX_SLOT, normal},
                                             {Trg::TexSampled, NOISE_TEX_SLOT, noise},
                                             {Trg::SBufRO, IN_RAY_LIST_SLOT, in_ray_list},
                                             {Trg::ImageRW, OUT_GI_IMG_SLOT, out_gi},
                                             {Trg::SBufRW, INOUT_RAY_COUNTER_SLOT, inout_ray_counter},
                                             {Trg::SBufRW, OUT_RAY_LIST_SLOT, out_ray_list}};

            Params uniform_params;
            uniform_params.resolution =
                Ren::Vec4u{uint32_t(view_state_.ren_res[0]), uint32_t(view_state_.ren_res[1]), 0, 0};

            DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_trace_ss_, fg.storages(), indir_args, 0, bindings,
                                    &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }

    if ((ctx_.capabilities.hwrt || ctx_.capabilities.swrt) && acc_structs.rt_tlas_buf[int(eTLASIndex::Main)]) {
        FgBufRWHandle indir_rt_disp;

        { // Prepare arguments for indirect RT dispatch
            auto &rt_disp_args = fg_builder_.AddNode("DIFF RT DISP ARGS");

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
                    rt_disp_args.AddStorageOutput("GI RT Dispatch Args", desc, Stg::ComputeShader);
            }

            rt_disp_args.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTDiffuseWriteIndirectArgs;

                const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                DispatchCompute(fg.cmd_buf(), pi_diffuse_write_indirect_[1], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                bindings, nullptr, 0, fg.descr_alloc(), fg.log());
            });
        }

        const bool two_bounces = (settings.gi_quality == eGIQuality::Ultra);

        FgBufRWHandle ray_hits;

        { // Trace gi rays
            auto &rt_diff = fg_builder_.AddNode(two_bounces ? "RT DIFF 1ST" : "RT DIFF");

            const auto stage = Stg::ComputeShader;

            auto *data = fg_builder_.AllocTempData<ExRTDiffuse::Args>();
            data->geo_data = rt_diff.AddStorageReadonlyInput(rt_geo_instances_res, stage);
            data->materials = rt_diff.AddStorageReadonlyInput(common_buffers.materials, stage);
            data->vtx_buf1 = rt_diff.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
            data->ndx_buf = rt_diff.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
            data->shared_data = rt_diff.AddUniformBufferInput(common_buffers.shared_data, stage);
            data->noise = rt_diff.AddTextureInput(noise, stage);
            data->depth = rt_diff.AddTextureInput(frame_textures.depth, stage);
            data->normal = rt_diff.AddTextureInput(frame_textures.normal, stage);
            data->ray_list = rt_diff.AddStorageReadonlyInput(ray_rt_list, stage);
            data->indir_args = rt_diff.AddIndirectBufferInput(indir_rt_disp);
            data->tlas_buf = rt_diff.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = acc_structs.swrt.rt_root_node;
                data->swrt.rt_blas = rt_diff.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
                data->swrt.prim_ndx = rt_diff.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
                data->swrt.mesh_instances = rt_diff.AddStorageReadonlyInput(rt_obj_instances_res, stage);
            }

            data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

            ray_counter = data->inout_ray_counter = rt_diff.AddStorageOutput(ray_counter, stage);

            { // Ray hit results
                FgBufDesc desc;
                desc.type = Ren::eBufType::Storage;
                desc.size =
                    RTDiffuse::RAY_HITS_STRIDE * view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

                ray_hits = data->out_ray_hits =
                    rt_diff.AddStorageOutput("GI RT Hits Buf", desc, Ren::eStage::ComputeShader);
            }

            rt_diff.make_executor<ExRTDiffuse>(&view_state_, &bindless, data);
        }

        { // Prepare arguments for shading dispatch
            auto &rt_shade_args = fg_builder_.AddNode(two_bounces ? "DIFF RT SHADE ARGS 1ST" : "DIFF RT SHADE ARGS");

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
                    rt_shade_args.AddStorageOutput("GI RT Shade Args", desc, Stg::ComputeShader);
            }

            rt_shade_args.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTDiffuseWriteIndirectArgs;

                const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                 {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                DispatchCompute(fg.cmd_buf(), pi_diffuse_write_indirect_[2], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                bindings, nullptr, 0, fg.descr_alloc(), fg.log());
            });
        }

        FgBufRWHandle secondary_ray_list;

        { // Shade ray hits
            auto &diff_shade = fg_builder_.AddNode(two_bounces ? "DIFF SHADE 1ST" : "DIFF SHADE");

            struct PassData {
                FgImgROHandle noise;
                FgBufROHandle shared_data;
                FgImgROHandle depth, normal;
                FgImgROHandle env;
                FgBufROHandle mesh_instances;
                FgBufROHandle geo_data;
                FgBufROHandle materials;
                FgBufROHandle vtx_data0_buf;
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
                FgImgRWHandle out_gi;
                FgBufRWHandle out_ray_list;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->noise = diff_shade.AddTextureInput(noise, Stg::ComputeShader);
            data->shared_data = diff_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth = diff_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->normal = diff_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->env = diff_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
            data->mesh_instances = diff_shade.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
            data->geo_data = diff_shade.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
            data->materials = diff_shade.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
            data->vtx_data0_buf = diff_shade.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
            data->ndx_buf = diff_shade.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
            data->lights = diff_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
            data->shadow_depth = diff_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
            data->shadow_color = diff_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
            data->ltc_luts = diff_shade.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);
            data->cells = diff_shade.AddStorageReadonlyInput(common_buffers.rt_cells, Stg::ComputeShader);
            data->items = diff_shade.AddStorageReadonlyInput(common_buffers.rt_items, Stg::ComputeShader);

            data->irradiance = diff_shade.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
            data->distance = diff_shade.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
            data->offset = diff_shade.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

            if (common_buffers.stoch_lights) {
                data->stoch_lights =
                    diff_shade.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
                data->light_nodes =
                    diff_shade.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
            }

            data->in_ray_hits = diff_shade.AddStorageReadonlyInput(ray_hits, Stg::ComputeShader);
            data->indir_args = diff_shade.AddIndirectBufferInput(indir_rt_disp);
            ray_counter = data->inout_ray_counter = diff_shade.AddStorageOutput(ray_counter, Stg::ComputeShader);
            gi_img = data->out_gi = diff_shade.AddStorageImageOutput(gi_img, Stg::ComputeShader);

            if (two_bounces) {
                // packed ray list
                FgBufDesc desc = {};
                desc.type = Ren::eBufType::Storage;
                desc.size =
                    view_state_.ren_res[0] * view_state_.ren_res[1] * RTDiffuse::RAY_LIST_STRIDE * sizeof(uint32_t);

                secondary_ray_list = data->out_ray_list =
                    diff_shade.AddStorageOutput("GI Secondary Ray List", desc, Stg::ComputeShader);
            }

            diff_shade.set_execute_cb([this, &bindless, two_bounces, data](const FgContext &fg) {
                using namespace RTDiffuse;

                const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
                const Ren::ImageROHandle env = fg.AccessROImage(data->env);
                const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(data->mesh_instances);
                const Ren::BufferROHandle geo_data = fg.AccessROBuffer(data->geo_data);
                const Ren::BufferROHandle materials = fg.AccessROBuffer(data->materials);
                const Ren::BufferROHandle vtx_data0_buf = fg.AccessROBuffer(data->vtx_data0_buf);
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

                const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

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
                    {Trg::UTBuf, NDX_BUF_SLOT, ndx_buf},
                    {Trg::SBufRO, LIGHTS_BUF_SLOT, lights},
                    {Trg::TexSampled, SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                    {Trg::TexSampled, SHADOW_COLOR_TEX_SLOT, shadow_color},
                    {Trg::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts},
                    {Trg::UTBuf, CELLS_BUF_SLOT, cells},
                    {Trg::UTBuf, ITEMS_BUF_SLOT, items},
                    {Trg::SBufRO, RAY_HITS_BUF_SLOT, in_ray_hits},
                    {Trg::SBufRW, RAY_COUNTER_SLOT, inout_ray_counter},
                    {Trg::ImageRW, OUT_GI_IMG_SLOT, out_gi}};

                RTDiffuse::Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
                uniform_params.frame_index = view_state_.frame_index;
                uniform_params.lights_count = view_state_.stochastic_lights_count;
                uniform_params.is_hwrt = ctx_.capabilities.hwrt ? 1 : 0;

                // Shade misses
                DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_shade_[0], fg.storages(), indir_args, 0, bindings,
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
                    two_bounces ? pi_diffuse_shade_[4 + int(view_state_.stochastic_lights_count != 0)]
                                : pi_diffuse_shade_[2 + int(view_state_.stochastic_lights_count != 0)];
                DispatchComputeIndirect(fg.cmd_buf(), pi, fg.storages(), indir_args,
                                        sizeof(Ren::DispatchIndirectCommand), bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });
        }

        if (two_bounces) {
            { // Prepare arguments for indirect RT dispatch
                auto &rt_disp_args = fg_builder_.AddNode("DIFF RT DISP ARGS 2ND");

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
                        rt_disp_args.AddStorageOutput("GI RT Secondary Dispatch Args", desc, Stg::ComputeShader);
                }

                rt_disp_args.set_execute_cb([this, data](const FgContext &fg) {
                    using namespace RTDiffuseWriteIndirectArgs;

                    const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                    const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                    const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                     {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                    DispatchCompute(fg.cmd_buf(), pi_diffuse_write_indirect_[1], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                    bindings, nullptr, 0, fg.descr_alloc(), fg.log());
                });
            }

            FgBufRWHandle secondary_ray_hits;

            { // Trace gi rays
                auto &rt_diff = fg_builder_.AddNode("RT DIFF 2ND");

                auto *data = fg_builder_.AllocTempData<ExRTDiffuse::Args>();
                data->second_bounce = true;

                const auto stage = Stg::ComputeShader;

                data->geo_data = rt_diff.AddStorageReadonlyInput(rt_geo_instances_res, stage);
                data->materials = rt_diff.AddStorageReadonlyInput(common_buffers.materials, stage);
                data->vtx_buf1 = rt_diff.AddStorageReadonlyInput(common_buffers.vertex_buf1, stage);
                data->ndx_buf = rt_diff.AddStorageReadonlyInput(common_buffers.indices_buf, stage);
                data->shared_data = rt_diff.AddUniformBufferInput(common_buffers.shared_data, stage);
                data->noise = rt_diff.AddTextureInput(noise, stage);
                data->depth = rt_diff.AddTextureInput(frame_textures.depth, stage);
                data->normal = rt_diff.AddTextureInput(frame_textures.normal, stage);
                data->ray_list = rt_diff.AddStorageReadonlyInput(secondary_ray_list, stage);
                data->indir_args = rt_diff.AddIndirectBufferInput(indir_rt_disp);
                data->tlas_buf = rt_diff.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)], stage);

                if (!ctx_.capabilities.hwrt) {
                    data->swrt.root_node = acc_structs.swrt.rt_root_node;
                    data->swrt.rt_blas = rt_diff.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, stage);
                    data->swrt.prim_ndx = rt_diff.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, stage);
                    data->swrt.mesh_instances = rt_diff.AddStorageReadonlyInput(rt_obj_instances_res, stage);
                }

                data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

                ray_counter = data->inout_ray_counter = rt_diff.AddStorageOutput(ray_counter, stage);

                { // Ray hit results
                    FgBufDesc desc;
                    desc.type = Ren::eBufType::Storage;
                    desc.size =
                        RTDiffuse::RAY_HITS_STRIDE * view_state_.ren_res[0] * view_state_.ren_res[1] * sizeof(uint32_t);

                    secondary_ray_hits = data->out_ray_hits =
                        rt_diff.AddStorageOutput("Diff RT Secondary Hits Buf", desc, Ren::eStage::ComputeShader);
                }

                rt_diff.make_executor<ExRTDiffuse>(&view_state_, &bindless, data);
            }

            { // Prepare arguments for shading dispatch
                auto &rt_shade_args = fg_builder_.AddNode("DIFF RT SHADE ARGS 2ND");

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
                        rt_shade_args.AddStorageOutput("GI RT Secondary Shade Args", desc, Stg::ComputeShader);
                }

                rt_shade_args.set_execute_cb([this, data](const FgContext &fg) {
                    using namespace RTDiffuseWriteIndirectArgs;

                    const Ren::BufferHandle ray_counter = fg.AccessRWBuffer(data->ray_counter);
                    const Ren::BufferHandle indir_disp = fg.AccessRWBuffer(data->indir_disp);

                    const Ren::Binding bindings[] = {{Trg::SBufRW, RAY_COUNTER_SLOT, ray_counter},
                                                     {Trg::SBufRW, INDIR_ARGS_SLOT, indir_disp}};

                    DispatchCompute(fg.cmd_buf(), pi_diffuse_write_indirect_[2], fg.storages(), Ren::Vec3u{1u, 1u, 1u},
                                    bindings, nullptr, 0, fg.descr_alloc(), fg.log());
                });
            }

            { // Shade ray hits
                auto &diff_shade = fg_builder_.AddNode("DIFF SHADE 2ND");

                struct PassData {
                    FgImgROHandle noise;
                    FgBufROHandle shared_data;
                    FgImgROHandle depth, normal;
                    FgImgROHandle env;
                    FgBufROHandle mesh_instances;
                    FgBufROHandle geo_data;
                    FgBufROHandle materials;
                    FgBufROHandle vtx_data0_buf;
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
                    FgImgRWHandle out_gi;
                };

                auto *data = fg_builder_.AllocTempData<PassData>();
                data->noise = diff_shade.AddTextureInput(noise, Stg::ComputeShader);
                data->shared_data = diff_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
                data->depth = diff_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
                data->normal = diff_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
                data->env = diff_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
                data->mesh_instances = diff_shade.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
                data->geo_data = diff_shade.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
                data->materials = diff_shade.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
                data->vtx_data0_buf =
                    diff_shade.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
                data->ndx_buf = diff_shade.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
                data->lights = diff_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
                data->shadow_depth = diff_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
                data->shadow_color = diff_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
                data->ltc_luts = diff_shade.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);
                data->cells = diff_shade.AddStorageReadonlyInput(common_buffers.rt_cells, Stg::ComputeShader);
                data->items = diff_shade.AddStorageReadonlyInput(common_buffers.rt_items, Stg::ComputeShader);

                data->irradiance = diff_shade.AddTextureInput(frame_textures.gi_cache_irradiance, Stg::ComputeShader);
                data->distance = diff_shade.AddTextureInput(frame_textures.gi_cache_distance, Stg::ComputeShader);
                data->offset = diff_shade.AddTextureInput(frame_textures.gi_cache_offset, Stg::ComputeShader);

                data->in_ray_list = diff_shade.AddStorageReadonlyInput(secondary_ray_list, Stg::ComputeShader);
                data->in_ray_hits = diff_shade.AddStorageReadonlyInput(secondary_ray_hits, Stg::ComputeShader);
                data->indir_args = diff_shade.AddIndirectBufferInput(indir_rt_disp);
                ray_counter = data->inout_ray_counter = diff_shade.AddStorageOutput(ray_counter, Stg::ComputeShader);
                gi_img = data->out_gi = diff_shade.AddStorageImageOutput(gi_img, Stg::ComputeShader);

                diff_shade.set_execute_cb([this, &bindless, data](const FgContext &fg) {
                    using namespace RTDiffuse;

                    const Ren::ImageROHandle noise = fg.AccessROImage(data->noise);
                    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                    const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                    const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
                    const Ren::ImageROHandle env = fg.AccessROImage(data->env);
                    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(data->mesh_instances);
                    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(data->geo_data);
                    const Ren::BufferROHandle materials = fg.AccessROBuffer(data->materials);
                    const Ren::BufferROHandle vtx_data0_buf = fg.AccessROBuffer(data->vtx_data0_buf);
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

                    const Ren::BufferRWHandle inout_ray_counter = fg.AccessRWBuffer(data->inout_ray_counter);
                    const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

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
                        {Trg::ImageRW, OUT_GI_IMG_SLOT, out_gi}};

                    RTDiffuse::Params uniform_params;
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;
                    uniform_params.frame_index = view_state_.frame_index;
                    uniform_params.lights_count = view_state_.stochastic_lights_count;
                    uniform_params.is_hwrt = ctx_.capabilities.hwrt ? 1 : 0;

                    // Shade misses
                    DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_shade_[1], fg.storages(), indir_args, 0, bindings,
                                            &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());

                    bindings.emplace_back(Trg::TexSampled, IRRADIANCE_TEX_SLOT, irr);
                    bindings.emplace_back(Trg::TexSampled, DISTANCE_TEX_SLOT, dist);
                    bindings.emplace_back(Trg::TexSampled, OFFSET_TEX_SLOT, off);

                    // Shade hits
                    DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_shade_[6], fg.storages(), indir_args,
                                            sizeof(Ren::DispatchIndirectCommand), bindings, &uniform_params,
                                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
                });
            }
        }

        { // Direct light sampling
            auto &sample_lights = fg_builder_.AddNode("SAMPLE LIGHTS");

            auto *data = fg_builder_.AllocTempData<ExSampleLights::Args>();
            data->shared_data = sample_lights.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->random_seq = sample_lights.AddStorageReadonlyInput(common_buffers.pmj_samples, Stg::ComputeShader);
            if (common_buffers.stoch_lights) {
                data->lights = sample_lights.AddStorageReadonlyInput(common_buffers.stoch_lights, Stg::ComputeShader);
                data->nodes =
                    sample_lights.AddStorageReadonlyInput(common_buffers.stoch_lights_nodes, Stg::ComputeShader);
            }

            data->geo_data = sample_lights.AddStorageReadonlyInput(rt_geo_instances_res, Stg::ComputeShader);
            data->materials = sample_lights.AddStorageReadonlyInput(common_buffers.materials, Stg::ComputeShader);
            data->vtx_buf1 = sample_lights.AddStorageReadonlyInput(common_buffers.vertex_buf1, Stg::ComputeShader);
            data->ndx_buf = sample_lights.AddStorageReadonlyInput(common_buffers.indices_buf, Stg::ComputeShader);
            data->tlas_buf = sample_lights.AddStorageReadonlyInput(acc_structs.rt_tlas_buf[int(eTLASIndex::Main)],
                                                                   Stg::ComputeShader);

            if (!ctx_.capabilities.hwrt) {
                data->swrt.root_node = acc_structs.swrt.rt_root_node;
                data->swrt.rt_blas_buf =
                    sample_lights.AddStorageReadonlyInput(acc_structs.swrt.rt_blas_buf, Stg::ComputeShader);
                data->swrt.prim_ndx =
                    sample_lights.AddStorageReadonlyInput(acc_structs.swrt.rt_prim_indices, Stg::ComputeShader);
                data->swrt.mesh_instances =
                    sample_lights.AddStorageReadonlyInput(rt_obj_instances_res, Stg::ComputeShader);
            }

            data->tlas = acc_structs.rt_tlases[int(eTLASIndex::Main)];

            data->albedo = sample_lights.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
            data->depth = sample_lights.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->norm = sample_lights.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->spec = sample_lights.AddTextureInput(frame_textures.specular, Stg::ComputeShader);

            gi_img = data->out_diffuse = sample_lights.AddStorageImageOutput(gi_img, Stg::ComputeShader);

            { // reflections texture
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;
                data->out_specular = sample_lights.AddStorageImageOutput("Spec Temp 2", desc, Stg::ComputeShader);
            }

            sample_lights.make_executor<ExSampleLights>(&view_state_, &bindless, data);
        }
    }

    if (debug_denoise) {
        frame_textures.gi_diffuse = gi_img;
        return;
    }

    FgImgRWHandle reproj_gi, avg_gi;
    FgImgRWHandle variance_temp, sample_count;

    { // Denoiser reprojection
        auto &diff_reproject = fg_builder_.AddNode("DIFF REPROJECT");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth, norm, velocity;
            FgImgROHandle depth_hist, norm_hist;
            FgImgROHandle variance_hist, sample_count_hist;
            FgImgROHandle gi, gi_hist;
            FgBufROHandle tile_list;
            FgBufROHandle indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_reprojected, out_avg_gi;
            FgImgRWHandle out_variance, out_sample_count;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = diff_reproject.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->depth = diff_reproject.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm = diff_reproject.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->velocity = diff_reproject.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
        data->depth_hist = diff_reproject.AddHistoryTextureInput(OPAQUE_DEPTH_TEX, Stg::ComputeShader);
        data->norm_hist = diff_reproject.AddHistoryTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->gi_hist = diff_reproject.AddHistoryTextureInput("GI Diffuse Filtered", Stg::ComputeShader);
        data->variance_hist = diff_reproject.AddHistoryTextureInput(DIFFUSE_VARIANCE_TEX, Stg::ComputeShader);
        data->gi = diff_reproject.AddTextureInput(gi_img, Stg::ComputeShader);

        data->tile_list = diff_reproject.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = diff_reproject.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Reprojected gi texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            reproj_gi = data->out_reprojected =
                diff_reproject.AddStorageImageOutput("GI Reprojected", desc, Stg::ComputeShader);
        }
        { // 8x8 average gi texture
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] + 7) / 8;
            desc.h = (view_state_.ren_res[1] + 7) / 8;
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            avg_gi = data->out_avg_gi = diff_reproject.AddStorageImageOutput("Average GI", desc, Stg::ComputeShader);
        }
        { // Variance
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            variance_temp = data->out_variance =
                diff_reproject.AddStorageImageOutput("GI Variance Temp", desc, Stg::ComputeShader);
        }
        { // Sample count
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            sample_count = data->out_sample_count =
                diff_reproject.AddStorageImageOutput("GI Sample Count", desc, Stg::ComputeShader);
        }

        data->sample_count_hist = diff_reproject.AddHistoryTextureInput(data->out_sample_count, Stg::ComputeShader);

        diff_reproject.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle shared_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
            const Ren::ImageROHandle depth_hist = fg.AccessROImage(data->depth_hist);
            const Ren::ImageROHandle norm_hist = fg.AccessROImage(data->norm_hist);
            const Ren::ImageROHandle gi_hist = fg.AccessROImage(data->gi_hist);
            const Ren::ImageROHandle variance_hist = fg.AccessROImage(data->variance_hist);
            const Ren::ImageROHandle sample_count_hist = fg.AccessROImage(data->sample_count_hist);
            const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_reprojected = fg.AccessRWImage(data->out_reprojected);
            const Ren::ImageRWHandle out_avg_gi = fg.AccessRWImage(data->out_avg_gi);
            const Ren::ImageRWHandle out_variance = fg.AccessRWImage(data->out_variance);
            const Ren::ImageRWHandle out_sample_count = fg.AccessRWImage(data->out_sample_count);

            { // Process tiles
                using namespace RTDiffuseReproject;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, shared_data},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity},
                                                 {Trg::TexSampled, DEPTH_HIST_TEX_SLOT, {depth_hist, 1}},
                                                 {Trg::TexSampled, NORM_HIST_TEX_SLOT, norm_hist},
                                                 {Trg::TexSampled, GI_HIST_TEX_SLOT, gi_hist},
                                                 {Trg::TexSampled, VARIANCE_HIST_TEX_SLOT, variance_hist},
                                                 {Trg::TexSampled, SAMPLE_COUNT_HIST_TEX_SLOT, sample_count_hist},
                                                 {Trg::TexSampled, GI_TEX_SLOT, gi},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_REPROJECTED_IMG_SLOT, out_reprojected},
                                                 {Trg::ImageRW, OUT_AVG_GI_IMG_SLOT, out_avg_gi},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance},
                                                 {Trg::ImageRW, OUT_SAMPLE_COUNT_IMG_SLOT, out_sample_count}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);
                uniform_params.unjitter = Ren::Vec2f{view_state_.jitter[0] - 0.5f, view_state_.jitter[1] - 0.5f};

                DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_reproject_, fg.storages(), indir_args,
                                        data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_reprojected},
                                                 {Trg::ImageRW, OUT_AVG_RAD_IMG_SLOT, out_avg_gi},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[3], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    FgImgRWHandle prefiltered_gi;

    { // Denoiser prefilter
        auto &diff_prefilter = fg_builder_.AddNode("DIFF PREFILTER");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle depth, spec, norm;
            FgImgROHandle gi, avg_gi;
            FgImgROHandle sample_count;
            FgBufROHandle tile_list;
            FgBufROHandle indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_gi;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = diff_prefilter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->spec = diff_prefilter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
        data->depth = diff_prefilter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->norm = diff_prefilter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->gi = diff_prefilter.AddTextureInput(gi_img, Stg::ComputeShader);
        data->avg_gi = diff_prefilter.AddTextureInput(avg_gi, Stg::ComputeShader);
        data->sample_count = diff_prefilter.AddTextureInput(sample_count, Stg::ComputeShader);
        data->tile_list = diff_prefilter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = diff_prefilter.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        { // Final diffuse
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            prefiltered_gi = data->out_gi =
                diff_prefilter.AddStorageImageOutput("GI Diffuse 1", desc, Stg::ComputeShader);
        }

        diff_prefilter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
            const Ren::ImageROHandle avg_gi = fg.AccessROImage(data->avg_gi);
            const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

            { // Filter tiles
                using namespace RTDiffuseFilter;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                 {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, GI_TEX_SLOT, {gi, nearest_sampler_}},
                                                 {Trg::TexSampled, AVG_GI_TEX_SLOT, avg_gi},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_gi}};

                Params uniform_params;
                uniform_params.rotator = view_state_.rand_rotators[0];
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_filter_[settings.taa_mode == eTAAMode::Static],
                                        fg.storages(), indir_args, data->indir_args_offset1, bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_gi}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    FgImgRWHandle gi_diffuse, gi_variance;

    { // Denoiser accumulation
        auto &diff_temporal = fg_builder_.AddNode("DIFF TEMPORAL");

        struct PassData {
            FgBufROHandle shared_data;
            FgImgROHandle norm;
            FgImgROHandle gi, avg_gi, reproj_gi;
            FgImgROHandle fallback_gi;
            FgImgROHandle variance, sample_count;
            FgBufROHandle tile_list;
            FgBufROHandle indir_args;
            uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
            FgImgRWHandle out_gi;
            FgImgRWHandle out_variance;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->shared_data = diff_temporal.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->norm = diff_temporal.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
        data->avg_gi = diff_temporal.AddTextureInput(avg_gi, Stg::ComputeShader);
        data->fallback_gi = diff_temporal.AddTextureInput(gi_fallback, Stg::ComputeShader);
        data->gi = diff_temporal.AddTextureInput(prefiltered_gi, Stg::ComputeShader);
        data->reproj_gi = diff_temporal.AddTextureInput(reproj_gi, Stg::ComputeShader);
        data->variance = diff_temporal.AddTextureInput(variance_temp, Stg::ComputeShader);
        data->sample_count = diff_temporal.AddTextureInput(sample_count, Stg::ComputeShader);
        data->tile_list = diff_temporal.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
        data->indir_args = diff_temporal.AddIndirectBufferInput(indir_disp);
        data->indir_args_offset1 = 3 * sizeof(uint32_t);
        data->indir_args_offset2 = 6 * sizeof(uint32_t);

        if (EnableBlur) {
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gi_diffuse = data->out_gi = diff_temporal.AddStorageImageOutput("GI Diffuse", desc, Stg::ComputeShader);
        } else {
            gi_diffuse = gi_img = data->out_gi = diff_temporal.AddStorageImageOutput(gi_img, Stg::ComputeShader);
        }
        { // Variance texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gi_variance = data->out_variance =
                diff_temporal.AddStorageImageOutput(DIFFUSE_VARIANCE_TEX, desc, Stg::ComputeShader);
        }

        diff_temporal.set_execute_cb([this, data, tile_count](const FgContext &fg) {
            const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
            const Ren::ImageROHandle avg_gi = fg.AccessROImage(data->avg_gi);
            const Ren::ImageROHandle fallback_gi = fg.AccessROImage(data->fallback_gi);
            const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
            const Ren::ImageROHandle reproj_gi = fg.AccessROImage(data->reproj_gi);
            const Ren::ImageROHandle variance = fg.AccessROImage(data->variance);
            const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
            const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
            const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

            const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);
            const Ren::ImageRWHandle out_variance = fg.AccessRWImage(data->out_variance);

            { // Process tiles
                using namespace RTDiffuseTemporal;

                const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                 {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                 {Trg::TexSampled, AVG_GI_TEX_SLOT, avg_gi},
                                                 {Trg::TexSampled, FALLBACK_TEX_SLOT, fallback_gi},
                                                 {Trg::TexSampled, GI_TEX_SLOT, gi},
                                                 {Trg::TexSampled, REPROJ_GI_TEX_SLOT, reproj_gi},
                                                 {Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                 {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                 {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_GI_IMG_SLOT, out_gi},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_temporal_[settings.taa_mode == eTAAMode::Static],
                                        fg.storages(), indir_args, data->indir_args_offset1, bindings, &uniform_params,
                                        sizeof(uniform_params), fg.descr_alloc(), fg.log());
            }
            { // Clear unused tiles
                using namespace TileClear;

                const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                 {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_gi},
                                                 {Trg::ImageRW, OUT_VARIANCE_IMG_SLOT, out_variance}};

                Params uniform_params;
                uniform_params.tile_count = tile_count;

                DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[2], fg.storages(), indir_args,
                                        data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                        fg.descr_alloc(), fg.log());
            }
        });
    }

    if (EnableBlur) {
        FgImgRWHandle gi_diffuse2;

        { // Denoiser blur
            auto &diff_filter = fg_builder_.AddNode("DIFF FILTER");

            struct PassData {
                FgBufROHandle shared_data;
                FgImgROHandle depth, spec, norm;
                FgImgROHandle gi;
                FgImgROHandle sample_count;
                FgBufROHandle tile_list;
                FgBufROHandle indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgImgRWHandle out_gi;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->shared_data = diff_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->spec = diff_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->depth = diff_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->norm = diff_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi = diff_filter.AddTextureInput(gi_diffuse, Stg::ComputeShader);
            data->sample_count = diff_filter.AddTextureInput(sample_count, Stg::ComputeShader);
            data->tile_list = diff_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = diff_filter.AddIndirectBufferInput(indir_disp);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final diffuse
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_diffuse2 = data->out_gi =
                    diff_filter.AddStorageImageOutput("GI Diffuse 2", desc, Stg::ComputeShader);
            }

            diff_filter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
                const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
                const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
                const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
                // const Ren::Image &variance = fg.AccessROImage(data->variance_tex);
                const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

                const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

                { // Filter tiles
                    using namespace RTDiffuseFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                     {Trg::TexSampled, GI_TEX_SLOT, {gi, nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                     //{Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_gi}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[0];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_filter_[2], fg.storages(), indir_args,
                                            data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_gi}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                            data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
            });
        }

        FgImgRWHandle gi_diffuse3;

        { // Denoiser blur 2
            auto &diff_post_filter = fg_builder_.AddNode("DIFF POST FILTER");

            struct PassData {
                FgBufROHandle shared_data;
                FgImgROHandle depth, spec, norm;
                FgImgROHandle gi, sample_count;
                FgBufROHandle tile_list;
                FgBufROHandle indir_args;
                uint32_t indir_args_offset1 = 0, indir_args_offset2 = 0;
                FgImgRWHandle out_gi;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->shared_data = diff_post_filter.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
            data->depth = diff_post_filter.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->spec = diff_post_filter.AddTextureInput(frame_textures.specular, Stg::ComputeShader);
            data->norm = diff_post_filter.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
            data->gi = diff_post_filter.AddTextureInput(gi_diffuse2, Stg::ComputeShader);
            data->sample_count = diff_post_filter.AddTextureInput(sample_count, Stg::ComputeShader);
            data->tile_list = diff_post_filter.AddStorageReadonlyInput(tile_list, Stg::ComputeShader);
            data->indir_args = diff_post_filter.AddIndirectBufferInput(indir_disp);
            data->indir_args_offset1 = 3 * sizeof(uint32_t);
            data->indir_args_offset2 = 6 * sizeof(uint32_t);

            { // Final diffuse
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_diffuse3 = data->out_gi =
                    diff_post_filter.AddStorageImageOutput("GI Diffuse Filtered", desc, Stg::ComputeShader);
            }

            diff_post_filter.set_execute_cb([this, data, tile_count](const FgContext &fg) {
                const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(data->shared_data);
                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);
                const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);
                const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
                const Ren::ImageROHandle sample_count = fg.AccessROImage(data->sample_count);
                // const Ren::Image &variance = fg.AccessROImage(data->variance);
                const Ren::BufferROHandle tile_list = fg.AccessROBuffer(data->tile_list);
                const Ren::BufferROHandle indir_args = fg.AccessROBuffer(data->indir_args);

                const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

                { // Filter tiles
                    using namespace RTDiffuseFilter;

                    const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                                     {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                     {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                                     {Trg::TexSampled, NORM_TEX_SLOT, norm},
                                                     {Trg::TexSampled, GI_TEX_SLOT, {gi, nearest_sampler_}},
                                                     {Trg::TexSampled, SAMPLE_COUNT_TEX_SLOT, sample_count},
                                                     //{Trg::TexSampled, VARIANCE_TEX_SLOT, variance},
                                                     {Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_DENOISED_IMG_SLOT, out_gi}};

                    Params uniform_params;
                    uniform_params.rotator = view_state_.rand_rotators[1];
                    uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
                    uniform_params.frame_index[0] = uint32_t(view_state_.frame_index) & 0xFFu;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_diffuse_filter_[3], fg.storages(), indir_args,
                                            data->indir_args_offset1, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
                { // Clear unused tiles
                    using namespace TileClear;

                    const Ren::Binding bindings[] = {{Trg::SBufRO, TILE_LIST_BUF_SLOT, tile_list},
                                                     {Trg::ImageRW, OUT_RAD_IMG_SLOT, out_gi}};

                    Params uniform_params;
                    uniform_params.tile_count = tile_count;

                    DispatchComputeIndirect(fg.cmd_buf(), pi_tile_clear_[0], fg.storages(), indir_args,
                                            data->indir_args_offset2, bindings, &uniform_params, sizeof(uniform_params),
                                            fg.descr_alloc(), fg.log());
                }
            });
        }

        FgImgRWHandle gi_diffuse4;

        { // Denoiser stabilization
            auto &diff_stabilization = fg_builder_.AddNode("DIFF STABILIZATION");

            struct PassData {
                FgImgROHandle depth, velocity;
                FgImgROHandle gi, gi_hist;
                FgImgRWHandle out_gi;
            };

            auto *data = fg_builder_.AllocTempData<PassData>();
            data->depth = diff_stabilization.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
            data->velocity = diff_stabilization.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);
            data->gi = diff_stabilization.AddTextureInput(gi_diffuse3, Stg::ComputeShader);

            { // Final gi
                FgImgDesc desc;
                desc.w = view_state_.ren_res[0];
                desc.h = view_state_.ren_res[1];
                desc.format = Ren::eFormat::RGBA16F;
                desc.sampling.filter = Ren::eFilter::Bilinear;
                desc.sampling.wrap = Ren::eWrap::ClampToEdge;

                gi_diffuse4 = data->out_gi =
                    diff_stabilization.AddStorageImageOutput("GI Diffuse 4", desc, Stg::ComputeShader);
            }

            data->gi_hist = diff_stabilization.AddHistoryTextureInput(gi_diffuse4, Stg::ComputeShader);

            diff_stabilization.set_execute_cb([this, data](const FgContext &fg) {
                using namespace RTDiffuseStabilization;

                const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
                const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
                const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
                const Ren::ImageROHandle gi_hist = fg.AccessROImage(data->gi_hist);

                const Ren::ImageRWHandle out_gi = fg.AccessRWImage(data->out_gi);

                const Ren::Binding bindings[] = {{Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, VELOCITY_TEX_SLOT, velocity},
                                                 {Trg::TexSampled, GI_TEX_SLOT, gi},
                                                 {Trg::TexSampled, GI_HIST_TEX_SLOT, gi_hist},
                                                 {Trg::ImageRW, OUT_GI_IMG_SLOT, out_gi}};

                const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_.ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                                        (view_state_.ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

                Params uniform_params;
                uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};

                DispatchCompute(fg.cmd_buf(), pi_diffuse_stabilization_, fg.storages(), grp_count, bindings,
                                &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
            });
        }

        if (EnableStabilization) {
            frame_textures.gi_diffuse = gi_diffuse4;
        } else {
            frame_textures.gi_diffuse = gi_diffuse3;
        }
    } else {
        frame_textures.gi_diffuse = gi_diffuse;
    }
}

Eng::FgImgRWHandle Eng::Renderer::AddSSAOPasses(const FgImgROHandle depth_down_2x, const FgImgROHandle _depth_tex) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgImgRWHandle ssao_raw;
    { // Main SSAO pass
        auto &ssao = fg_builder_.AddNode("SSAO");

        struct PassData {
            FgImgROHandle rand;
            FgImgROHandle depth;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        // data->rand = ssao.AddTextureInput(rand2d_dirs_4x4_, Stg::FragmentShader);
        data->depth = ssao.AddTextureInput(depth_down_2x, Stg::FragmentShader);

        { // Allocate output texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0] / 2;
            desc.h = view_state_.ren_res[1] / 2;
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssao_raw = data->output = ssao.AddColorOutput("SSAO RAW", desc);
        }

        ssao.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle down_depth_2x = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle rand = fg.AccessROImage(data->rand);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            const Ren::Binding bindings[] = {{Trg::TexSampled, SSAO::DEPTH_TEX_SLOT, down_depth_2x},
                                             {Trg::TexSampled, SSAO::RAND_TEX_SLOT, rand}};

            SSAO::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_.ren_res[0] / 2, view_state_.ren_res[1] / 2};
            uniform_params.resolution = Ren::Vec2f{view_state_.ren_res};

            const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

            prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_ao_prog_, {}, render_targets, rast_state,
                                fg.rast_state(), bindings, &uniform_params, sizeof(SSAO::Params), 0, fg.framebuffers());
        });
    }

    FgImgRWHandle ssao_blurred1;
    { // Horizontal SSAO blur
        auto &ssao_blur_h = fg_builder_.AddNode("SSAO BLUR H");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle input;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = ssao_blur_h.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->input = ssao_blur_h.AddTextureInput(ssao_raw, Stg::FragmentShader);

        { // Allocate output texture
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssao_blurred1 = data->output = ssao_blur_h.AddColorOutput("SSAO BLUR TEMP1", desc);
        }

        ssao_blur_h.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // blur ao buffer
                const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::TexSampled, Bilateral::DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, Bilateral::INPUT_TEX_SLOT, input}};

                Bilateral::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution = Ren::Vec2f{float(rast_state.viewport[2]), float(rast_state.viewport[3])};
                uniform_params.vertical = 0.0f;

                prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_bilateral_prog_, {}, render_targets,
                                    rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params),
                                    0, fg.framebuffers());
            }
        });
    }

    FgImgRWHandle ssao_blurred2;
    { // Vertical SSAO blur
        auto &ssao_blur_v = fg_builder_.AddNode("SSAO BLUR V");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle input;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = ssao_blur_v.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->input = ssao_blur_v.AddTextureInput(ssao_blurred1, Stg::FragmentShader);

        { // Allocate output texture
            FgImgDesc desc;
            desc.w = (view_state_.ren_res[0] / 2);
            desc.h = (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            ssao_blurred2 = data->output = ssao_blur_v.AddColorOutput("SSAO BLUR TEMP2", desc);
        }

        ssao_blur_v.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle input = fg.AccessROImage(data->input);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = (view_state_.ren_res[0] / 2);
            rast_state.viewport[3] = (view_state_.ren_res[1] / 2);

            { // blur ao buffer
                const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                const Ren::Binding bindings[] = {{Trg::TexSampled, Bilateral::DEPTH_TEX_SLOT, {depth, 1}},
                                                 {Trg::TexSampled, Bilateral::INPUT_TEX_SLOT, input}};

                Bilateral::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution = Ren::Vec2f{float(rast_state.viewport[2]), float(rast_state.viewport[3])};
                uniform_params.vertical = 1.0f;

                prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_bilateral_prog_, {}, render_targets,
                                    rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(Bilateral::Params),
                                    0, fg.framebuffers());
            }
        });
    }

    FgImgRWHandle out_ssao;

    { // Upscale SSAO pass
        auto &ssao_upscale = fg_builder_.AddNode("UPSCALE");

        struct PassData {
            FgImgROHandle depth_down_2x;
            FgImgROHandle depth;
            FgImgROHandle input;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth_down_2x = ssao_upscale.AddTextureInput(depth_down_2x, Stg::FragmentShader);
        data->depth = ssao_upscale.AddTextureInput(_depth_tex, Stg::FragmentShader);
        data->input = ssao_upscale.AddTextureInput(ssao_blurred2, Stg::FragmentShader);

        { // Allocate output texture
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            // out_ssao = data->output = ssao_upscale.AddColorOutputNew("SSAO Final", desc);
        }

        ssao_upscale.set_execute_cb([data](const FgContext &fg) {
            [[maybe_unused]] const Ren::ImageROHandle down_depth_2x = fg.AccessROImage(data->depth_down_2x);
            [[maybe_unused]] const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            [[maybe_unused]] const Ren::ImageROHandle input = fg.AccessROImage(data->input);

            [[maybe_unused]] const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            /*Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
            rast_state.viewport[2] = view_state_.ren_res[0];
            rast_state.viewport[3] = view_state_.ren_res[1];
            { // upsample ao
                const Ren::RenderTarget render_targets[] = {{output_tex, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};
                const Ren::Binding bindings[] = {{Trg::TexSampled, Upscale::DEPTH_TEX_SLOT, {depth_tex, 1}},
                                                 {Trg::TexSampled, Upscale::DEPTH_LOW_TEX_SLOT, down_depth_2x_tex},
                                                 {Trg::TexSampled, Upscale::INPUT_TEX_SLOT, input_tex}};
                Upscale::Params uniform_params;
                uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
                uniform_params.resolution =
                    Ren::Vec4f{float(view_state_.ren_res[0]), float(view_state_.ren_res[1]), 0.0f, 0.0f};
                uniform_params.clip_info = view_state_.clip_info;
                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_upscale_prog_, {}, render_targets, rast_state,
                                    fg.rast_state(), bindings, &uniform_params, sizeof(Upscale::Params), 0);
            }*/
        });
    }

    return out_ssao;
}

Eng::FgImgRWHandle Eng::Renderer::AddGTAOPasses(const eSSAOQuality quality, const FgImgROHandle depth_tex,
                                                const FgImgROHandle velocity_tex, const FgImgROHandle norm_tex) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    FgImgRWHandle gtao_result;
    { // main pass
        auto &gtao_main = fg_builder_.AddNode("GTAO MAIN");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle norm;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = gtao_main.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->norm = gtao_main.AddTextureInput(norm_tex, Stg::ComputeShader);

        { // Output texture
            FgImgDesc desc;
            desc.w = (quality == eSSAOQuality::Ultra) ? view_state_.ren_res[0] : (view_state_.ren_res[0] / 2);
            desc.h = (quality == eSSAOQuality::Ultra) ? view_state_.ren_res[1] : (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gtao_result = data->output = gtao_main.AddStorageImageOutput("GTAO RAW", desc, Stg::ComputeShader);
        }

        gtao_main.set_execute_cb([this, data, quality](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle norm = fg.AccessROImage(data->norm);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Trg::TexSampled, GTAO::DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, GTAO::NORM_TEX_SLOT, norm},
                                             {Trg::ImageRW, GTAO::OUT_IMG_SLOT, output}};

            const Ren::Vec2u img_size{quality == eSSAOQuality::Ultra ? uint32_t(view_state_.ren_res[0])
                                                                     : uint32_t(view_state_.ren_res[0] / 2),
                                      quality == eSSAOQuality::Ultra ? uint32_t(view_state_.ren_res[1])
                                                                     : uint32_t(view_state_.ren_res[1] / 2)};

            const Ren::Vec3u grp_count{(img_size[0] + GTAO::GRP_SIZE_X - 1u) / GTAO::GRP_SIZE_X,
                                       (img_size[1] + GTAO::GRP_SIZE_Y - 1u) / GTAO::GRP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = img_size;
            uniform_params.rand[0] = RendererInternal::GTAORandSamples[view_state_.frame_index % 32][0];
            uniform_params.rand[1] = RendererInternal::GTAORandSamples[view_state_.frame_index % 32][1];
            uniform_params.clip_info = view_state_.clip_info;
            uniform_params.frustum_info = view_state_.frustum_info;
            uniform_params.view_from_world = view_state_.view_from_world;

            DispatchCompute(fg.cmd_buf(), pi_gtao_main_[quality == eSSAOQuality::High], fg.storages(), grp_count,
                            bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    { // filter pass
        auto &gtao_filter = fg_builder_.AddNode("GTAO FILTER");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle ao;
            FgImgRWHandle out_ao;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = gtao_filter.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->ao = gtao_filter.AddTextureInput(gtao_result, Stg::ComputeShader);

        { // Output texture
            FgImgDesc desc;
            desc.w = (quality == eSSAOQuality::Ultra) ? view_state_.ren_res[0] : (view_state_.ren_res[0] / 2);
            desc.h = (quality == eSSAOQuality::Ultra) ? view_state_.ren_res[1] : (view_state_.ren_res[1] / 2);
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gtao_result = data->out_ao = gtao_filter.AddStorageImageOutput("GTAO FILTERED", desc, Stg::ComputeShader);
        }

        gtao_filter.set_execute_cb([this, data, quality](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle ao = fg.AccessROImage(data->ao);

            const Ren::ImageRWHandle out_ao = fg.AccessRWImage(data->out_ao);

            const Ren::Binding bindings[] = {{Trg::TexSampled, GTAO::DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, GTAO::GTAO_TEX_SLOT, ao},
                                             {Trg::ImageRW, GTAO::OUT_IMG_SLOT, out_ao}};

            const Ren::Vec2u img_size{quality == eSSAOQuality::Ultra ? uint32_t(view_state_.ren_res[0])
                                                                     : uint32_t(view_state_.ren_res[0] / 2),
                                      quality == eSSAOQuality::Ultra ? uint32_t(view_state_.ren_res[1])
                                                                     : uint32_t(view_state_.ren_res[1] / 2)};

            const auto grp_count = Ren::Vec3u{(img_size[0] + GTAO::GRP_SIZE_X - 1u) / GTAO::GRP_SIZE_X,
                                              (img_size[1] + GTAO::GRP_SIZE_Y - 1u) / GTAO::GRP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = img_size;
            uniform_params.clip_info = view_state_.clip_info;

            DispatchCompute(fg.cmd_buf(), pi_gtao_filter_[quality == eSSAOQuality::High], fg.storages(), grp_count,
                            bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    { // accumulation pass
        auto &gtao_accumulation = fg_builder_.AddNode("GTAO ACCUMULATE");

        struct PassData {
            FgImgROHandle depth, depth_hist, velocity;
            FgImgROHandle ao, ao_hist;
            FgImgRWHandle out_ao;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = gtao_accumulation.AddTextureInput(depth_tex, Stg::ComputeShader);
        data->depth_hist = gtao_accumulation.AddHistoryTextureInput(depth_tex, Stg::ComputeShader);
        data->velocity = gtao_accumulation.AddTextureInput(velocity_tex, Stg::ComputeShader);
        data->ao = gtao_accumulation.AddTextureInput(gtao_result, Stg::ComputeShader);

        { // Final ao
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            gtao_result = data->out_ao =
                gtao_accumulation.AddStorageImageOutput("GTAO FINAL", desc, Stg::ComputeShader);
        }

        data->ao_hist = gtao_accumulation.AddHistoryTextureInput(gtao_result, Stg::ComputeShader);

        gtao_accumulation.set_execute_cb([this, data, quality](const FgContext &fg) {
            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle depth_hist = fg.AccessROImage(data->depth_hist);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);
            const Ren::ImageROHandle ao = fg.AccessROImage(data->ao);
            const Ren::ImageROHandle ao_hist = fg.AccessROImage(data->ao_hist);

            const Ren::ImageRWHandle out_ao = fg.AccessRWImage(data->out_ao);

            const Ren::Binding bindings[] = {{Trg::TexSampled, GTAO::DEPTH_TEX_SLOT, {depth, 1}},
                                             {Trg::TexSampled, GTAO::DEPTH_HIST_TEX_SLOT, {depth_hist, 1}},
                                             {Trg::TexSampled, GTAO::VELOCITY_TEX_SLOT, velocity},
                                             {Trg::TexSampled, GTAO::GTAO_TEX_SLOT, ao},
                                             {Trg::TexSampled, GTAO::GTAO_HIST_TEX_SLOT, ao_hist},
                                             {Trg::ImageRW, GTAO::OUT_IMG_SLOT, out_ao}};

            const Ren::Vec3u grp_count =
                Ren::Vec3u{(view_state_.ren_res[0] + GTAO::GRP_SIZE_X - 1u) / GTAO::GRP_SIZE_X,
                           (view_state_.ren_res[1] + GTAO::GRP_SIZE_Y - 1u) / GTAO::GRP_SIZE_Y, 1u};

            GTAO::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{view_state_.ren_res};
            uniform_params.clip_info = view_state_.clip_info;

            DispatchCompute(fg.cmd_buf(), pi_gtao_accumulate_[quality != eSSAOQuality::Ultra], fg.storages(), grp_count,
                            bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
        });
    }
    return gtao_result;
}
