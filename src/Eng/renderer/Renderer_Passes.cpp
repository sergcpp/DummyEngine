#include "Renderer.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/ScopeExit.h>
#include <Sys/Time_.h>

#include "../utils/Random.h"
#include "../utils/ShaderLoader.h"
#include "Renderer_Names.h"

#include "shaders/blit_bilateral_interface.h"
#include "shaders/blit_down_depth_interface.h"
#include "shaders/blit_down_interface.h"
#include "shaders/blit_gauss_interface.h"
#include "shaders/blit_ssao_interface.h"
#include "shaders/blit_static_vel_interface.h"
#include "shaders/blit_taa_interface.h"
#include "shaders/blit_upscale_interface.h"
#include "shaders/bloom_interface.h"
#include "shaders/debug_velocity_interface.h"
#include "shaders/gbuffer_shade_interface.h"
#include "shaders/gtao_interface.h"
#include "shaders/histogram_exposure_interface.h"
#include "shaders/histogram_sample_interface.h"
#include "shaders/skydome_interface.h"
#include "shaders/sun_brightness_interface.h"

namespace RendererInternal {
const float GoldenRatio = 1.61803398875f;
extern const int TaaSampleCountStatic;

static const float GTAORandSamples[32][2] = {
    {0.673997f, 0.678703f}, {0.381107f, 0.299157f}, {0.830422f, 0.123435f}, {0.110746f, 0.927141f},
    {0.913511f, 0.797823f}, {0.160077f, 0.141460f}, {0.557984f, 0.453895f}, {0.323667f, 0.502007f},
    {0.597559f, 0.967611f}, {0.284918f, 0.020696f}, {0.943984f, 0.367100f}, {0.228963f, 0.742073f},
    {0.794414f, 0.611045f}, {0.025854f, 0.406871f}, {0.695394f, 0.243437f}, {0.476599f, 0.826670f},
    {0.502447f, 0.539025f}, {0.353506f, 0.469774f}, {0.895886f, 0.159506f}, {0.131713f, 0.774900f},
    {0.857590f, 0.887409f}, {0.067422f, 0.090030f}, {0.648406f, 0.253075f}, {0.435085f, 0.632276f},
    {0.743669f, 0.861615f}, {0.442667f, 0.211039f}, {0.759943f, 0.403822f}, {0.037858f, 0.585661f},
    {0.978491f, 0.693668f}, {0.195222f, 0.323286f}, {0.566908f, 0.055406f}, {0.256133f, 0.988877f}};
} // namespace RendererInternal

void Eng::Renderer::InitPipelines() {
    auto subgroup_select = [this](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
        return ctx_.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
    };

    pi_gbuf_shade_[0] = sh_.LoadPipeline(subgroup_select("internal/gbuffer_shade@SS_SHADOW_ONE.comp.glsl",
                                                         "internal/gbuffer_shade@SS_SHADOW_ONE;NO_SUBGROUP.comp.glsl"));
    pi_gbuf_shade_[1] =
        sh_.LoadPipeline(subgroup_select("internal/gbuffer_shade@SHADOW_JITTER;SS_SHADOW_MANY.comp.glsl",
                                         "internal/gbuffer_shade@SHADOW_JITTER;SS_SHADOW_MANY;NO_SUBGROUP.comp.glsl"));
    pi_ssr_classify_ = sh_.LoadPipeline(subgroup_select("internal/ssr_classify.comp.glsl", //
                                                        "internal/ssr_classify@NO_SUBGROUP.comp.glsl"));
    pi_ssr_write_indirect_ = sh_.LoadPipeline("internal/ssr_write_indirect_args.comp.glsl");
    pi_ssr_trace_hq_[0][0] = sh_.LoadPipeline(subgroup_select("internal/ssr_trace_hq.comp.glsl", //
                                                              "internal/ssr_trace_hq@NO_SUBGROUP.comp.glsl"));
    pi_ssr_trace_hq_[0][1] = sh_.LoadPipeline(subgroup_select("internal/ssr_trace_hq@GI_CACHE.comp.glsl",
                                                              "internal/ssr_trace_hq@GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_ssr_trace_hq_[1][0] = sh_.LoadPipeline(subgroup_select("internal/ssr_trace_hq@LAYERED.comp.glsl",
                                                              "internal/ssr_trace_hq@LAYERED;NO_SUBGROUP.comp.glsl"));
    pi_ssr_trace_hq_[1][1] =
        sh_.LoadPipeline(subgroup_select("internal/ssr_trace_hq@LAYERED;GI_CACHE.comp.glsl",
                                         "internal/ssr_trace_hq@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_rt_write_indirect_ = sh_.LoadPipeline("internal/ssr_write_indir_rt_dispatch.comp.glsl");

    // Reflections denoising
    pi_ssr_reproject_ = sh_.LoadPipeline("internal/ssr_reproject.comp.glsl");
    pi_ssr_temporal_[0] = sh_.LoadPipeline("internal/ssr_temporal.comp.glsl");
    pi_ssr_temporal_[1] = sh_.LoadPipeline("internal/ssr_temporal@RELAXED.comp.glsl");
    pi_ssr_filter_[0] = sh_.LoadPipeline("internal/ssr_filter@PRE_FILTER.comp.glsl");
    pi_ssr_filter_[1] = sh_.LoadPipeline("internal/ssr_filter@PRE_FILTER;RELAXED.comp.glsl");
    pi_ssr_filter_[2] = sh_.LoadPipeline("internal/ssr_filter.comp.glsl");
    pi_ssr_filter_[3] = sh_.LoadPipeline("internal/ssr_filter@POST_FILTER.comp.glsl");
    pi_ssr_stabilization_ = sh_.LoadPipeline("internal/ssr_stabilization.comp.glsl");
    pi_tile_clear_[0] = sh_.LoadPipeline("internal/tile_clear.comp.glsl");
    pi_tile_clear_[1] = sh_.LoadPipeline("internal/tile_clear@AVERAGE.comp.glsl");
    pi_tile_clear_[2] = sh_.LoadPipeline("internal/tile_clear@VARIANCE.comp.glsl");
    pi_tile_clear_[3] = sh_.LoadPipeline("internal/tile_clear@AVERAGE;VARIANCE.comp.glsl");

    // GI Cache
    pi_probe_blend_[0][0] = sh_.LoadPipeline("internal/probe_blend@RADIANCE.comp.glsl");
    pi_probe_blend_[1][0] = sh_.LoadPipeline("internal/probe_blend@RADIANCE;STOCH_LIGHTS.comp.glsl");
    pi_probe_blend_[2][0] = sh_.LoadPipeline("internal/probe_blend@DISTANCE.comp.glsl");
    pi_probe_blend_[0][1] = sh_.LoadPipeline("internal/probe_blend@RADIANCE;PARTIAL.comp.glsl");
    pi_probe_blend_[1][1] = sh_.LoadPipeline("internal/probe_blend@RADIANCE;STOCH_LIGHTS;PARTIAL.comp.glsl");
    pi_probe_blend_[2][1] = sh_.LoadPipeline("internal/probe_blend@DISTANCE;PARTIAL.comp.glsl");
    pi_probe_relocate_[0] = sh_.LoadPipeline("internal/probe_relocate.comp.glsl");
    pi_probe_relocate_[1] = sh_.LoadPipeline("internal/probe_relocate@PARTIAL.comp.glsl");
    pi_probe_relocate_[2] = sh_.LoadPipeline("internal/probe_relocate@RESET.comp.glsl");
    pi_probe_classify_[0] = sh_.LoadPipeline("internal/probe_classify.comp.glsl");
    pi_probe_classify_[1] = sh_.LoadPipeline("internal/probe_classify@PARTIAL.comp.glsl");
    pi_probe_classify_[2] = sh_.LoadPipeline("internal/probe_classify@RESET.comp.glsl");
    pi_probe_sample_ = sh_.LoadPipeline("internal/probe_sample.comp.glsl");

    // GTAO
    pi_gtao_main_[0] = sh_.LoadPipeline("internal/gtao_main.comp.glsl");
    pi_gtao_main_[1] = sh_.LoadPipeline("internal/gtao_main@HALF_RES.comp.glsl");
    pi_gtao_filter_[0] = sh_.LoadPipeline("internal/gtao_filter.comp.glsl");
    pi_gtao_filter_[1] = sh_.LoadPipeline("internal/gtao_filter@HALF_RES.comp.glsl");
    pi_gtao_accumulate_ = sh_.LoadPipeline("internal/gtao_accumulate.comp.glsl");
    pi_gtao_upsample_ = sh_.LoadPipeline("internal/gtao_upsample.comp.glsl");

    // GI
    pi_gi_classify_ = sh_.LoadPipeline(subgroup_select("internal/gi_classify.comp.glsl", //
                                                       "internal/gi_classify@NO_SUBGROUP.comp.glsl"));
    pi_gi_write_indirect_ = sh_.LoadPipeline("internal/gi_write_indirect_args.comp.glsl");
    pi_gi_trace_ss_ = sh_.LoadPipeline(subgroup_select("internal/gi_trace_ss.comp.glsl", //
                                                       "internal/gi_trace_ss@NO_SUBGROUP.comp.glsl"));
    pi_gi_rt_write_indirect_ = sh_.LoadPipeline("internal/gi_write_indir_rt_dispatch.comp.glsl");
    pi_gi_reproject_ = sh_.LoadPipeline("internal/gi_reproject.comp.glsl");
    pi_gi_temporal_[0] = sh_.LoadPipeline("internal/gi_temporal.comp.glsl");
    pi_gi_temporal_[1] = sh_.LoadPipeline("internal/gi_temporal@RELAXED.comp.glsl");
    pi_gi_filter_[0] = sh_.LoadPipeline("internal/gi_filter@PRE_FILTER.comp.glsl");
    pi_gi_filter_[1] = sh_.LoadPipeline("internal/gi_filter@PRE_FILTER;RELAXED.comp.glsl");
    pi_gi_filter_[2] = sh_.LoadPipeline("internal/gi_filter.comp.glsl");
    pi_gi_filter_[3] = sh_.LoadPipeline("internal/gi_filter@POST_FILTER.comp.glsl");
    pi_gi_stabilization_ = sh_.LoadPipeline("internal/gi_stabilization.comp.glsl");

    // Sun Shadow
    pi_sun_shadows_ = sh_.LoadPipeline("internal/sun_shadows@SS_SHADOW.comp.glsl");
    pi_sun_brightness_ = sh_.LoadPipeline("internal/sun_brightness.comp.glsl");
    pi_shadow_classify_ = sh_.LoadPipeline(subgroup_select("internal/rt_shadow_classify.comp.glsl", //
                                                           "internal/rt_shadow_classify@NO_SUBGROUP.comp.glsl"),
                                           32);
    pi_shadow_prepare_mask_ = sh_.LoadPipeline(subgroup_select("internal/rt_shadow_prepare_mask.comp.glsl",
                                                               "internal/rt_shadow_prepare_mask@NO_SUBGROUP.comp.glsl"),
                                               32);
    pi_shadow_classify_tiles_ =
        sh_.LoadPipeline(subgroup_select("internal/rt_shadow_classify_tiles.comp.glsl",
                                         "internal/rt_shadow_classify_tiles@NO_SUBGROUP.comp.glsl"),
                         32);
    pi_shadow_filter_[0] = sh_.LoadPipeline("internal/rt_shadow_filter@PASS_0.comp.glsl");
    pi_shadow_filter_[1] = sh_.LoadPipeline("internal/rt_shadow_filter@PASS_1.comp.glsl");
    pi_shadow_filter_[2] = sh_.LoadPipeline("internal/rt_shadow_filter.comp.glsl");
    pi_shadow_debug_ = sh_.LoadPipeline("internal/rt_shadow_debug.comp.glsl");

    // Bloom
    pi_bloom_downsample_[0] = sh_.LoadPipeline("internal/bloom_downsample.comp.glsl");
    pi_bloom_downsample_[1] = sh_.LoadPipeline("internal/bloom_downsample@TONEMAP.comp.glsl");
    pi_bloom_upsample_ = sh_.LoadPipeline("internal/bloom_upsample.comp.glsl");

    // Autoexposure
    pi_histogram_sample_ = sh_.LoadPipeline("internal/histogram_sample.comp.glsl");
    pi_histogram_exposure_ = sh_.LoadPipeline("internal/histogram_exposure.comp.glsl");

    // Sky
    pi_sky_upsample_ = sh_.LoadPipeline("internal/skydome_upsample.comp.glsl");

    // Debugging
    pi_debug_velocity_ = sh_.LoadPipeline("internal/debug_velocity.comp.glsl");

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    blit_static_vel_prog_ = sh_.LoadProgram("internal/blit_static_vel.vert.glsl", "internal/blit_static_vel.frag.glsl");
    blit_gauss2_prog_ = sh_.LoadProgram("internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
    blit_ao_prog_ = sh_.LoadProgram("internal/blit_ssao.vert.glsl", "internal/blit_ssao.frag.glsl");
    blit_bilateral_prog_ = sh_.LoadProgram("internal/blit_bilateral.vert.glsl", "internal/blit_bilateral.frag.glsl");
    blit_taa_prog_[0] = sh_.LoadProgram("internal/blit_taa.vert.glsl",
                                        "internal/blit_taa@CATMULL_ROM;ROUNDED_NEIBOURHOOD;TONEMAP;YCoCg.frag.glsl");
    blit_taa_prog_[1] =
        sh_.LoadProgram("internal/blit_taa.vert.glsl",
                        "internal/blit_taa@CATMULL_ROM;ROUNDED_NEIBOURHOOD;TONEMAP;YCoCg;MOTION_BLUR.frag.glsl");
    blit_taa_static_prog_ =
        sh_.LoadProgram("internal/blit_taa.vert.glsl", "internal/blit_taa@STATIC_ACCUMULATION.frag.glsl");
    blit_ssr_prog_ = sh_.LoadProgram("internal/blit_ssr.vert.glsl", "internal/blit_ssr.frag.glsl");
    blit_ssr_dilate_prog_ = sh_.LoadProgram("internal/blit_ssr_dilate.vert.glsl", "internal/blit_ssr_dilate.frag.glsl");
    blit_ssr_compose_prog_ =
        sh_.LoadProgram("internal/blit_ssr_compose.vert.glsl", "internal/blit_ssr_compose.frag.glsl");
    blit_upscale_prog_ = sh_.LoadProgram("internal/blit_upscale.vert.glsl", "internal/blit_upscale.frag.glsl");
    blit_down2_prog_ = sh_.LoadProgram("internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
    blit_down_depth_prog_ = sh_.LoadProgram("internal/blit_down_depth.vert.glsl", "internal/blit_down_depth.frag.glsl");
}

void Eng::Renderer::AddBuffersUpdatePass(CommonBuffers &common_buffers, const PersistentGpuData &persistent_data) {
    auto &update_bufs = fg_builder_.AddNode("UPDATE BUFFERS");

    { // create skin transforms buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = SkinTransformsBufChunkSize;
        common_buffers.skin_transforms = update_bufs.AddTransferOutput("Skin Transforms", desc);
    }
    { // create shape keys buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ShapeKeysBufChunkSize;
        common_buffers.shape_keys = update_bufs.AddTransferOutput("Shape Keys", desc);
    }
    { // create instance indices buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceIndicesBufChunkSize;
        common_buffers.instance_indices = update_bufs.AddTransferOutput("Instance Indices", desc);
    }
    FgResRef shared_data_res;
    { // create uniform buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Uniform;
        desc.size = SharedDataBlockSize;
        shared_data_res = common_buffers.shared_data = update_bufs.AddTransferOutput("Shared Data", desc);
    }
    { // create atomic counter buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Storage;
        desc.size = sizeof(uint32_t);
        common_buffers.atomic_cnt = update_bufs.AddTransferOutput("Atomic Counter", desc);
    }

    update_bufs.set_execute_cb([this, &common_buffers, &persistent_data, shared_data_res](FgBuilder &builder) {
        Ren::Context &ctx = builder.ctx();
        FgAllocBuf &skin_transforms_buf = builder.GetWriteBuffer(common_buffers.skin_transforms);
        FgAllocBuf &shape_keys_buf = builder.GetWriteBuffer(common_buffers.shape_keys);
        // FgAllocBuf &instances_buf = builder.GetWriteBuffer(common_buffers.instances_res);
        FgAllocBuf &instance_indices_buf = builder.GetWriteBuffer(common_buffers.instance_indices);
        FgAllocBuf &shared_data_buf = builder.GetWriteBuffer(shared_data_res);
        FgAllocBuf &atomic_cnt_buf = builder.GetWriteBuffer(common_buffers.atomic_cnt);

        Ren::UpdateBuffer(
            *skin_transforms_buf.ref, 0, uint32_t(p_list_->skin_transforms.size() * sizeof(SkinTransform)),
            p_list_->skin_transforms.data(), *p_list_->skin_transforms_stage_buf,
            ctx.backend_frame() * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize, ctx.current_cmd_buf());

        Ren::UpdateBuffer(*shape_keys_buf.ref, 0, p_list_->shape_keys_data.count * sizeof(ShapeKeyData),
                          p_list_->shape_keys_data.data, *p_list_->shape_keys_stage_buf,
                          ctx.backend_frame() * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize, ctx.current_cmd_buf());

        if (!instance_indices_buf.tbos[0]) {
            instance_indices_buf.tbos[0] = ctx.CreateTexture1D("Instance Indices TBO", instance_indices_buf.ref,
                                                               Ren::eTexFormat::RG32UI, 0, InstanceIndicesBufChunkSize);
        }

        Ren::UpdateBuffer(*instance_indices_buf.ref, 0, uint32_t(p_list_->instance_indices.size() * sizeof(Ren::Vec2i)),
                          p_list_->instance_indices.data(), *p_list_->instance_indices_stage_buf,
                          ctx.backend_frame() * InstanceIndicesBufChunkSize, InstanceIndicesBufChunkSize,
                          ctx.current_cmd_buf());

        { // Prepare data that is shared for all instances
            SharedDataBlock shrd_data;

            shrd_data.view_from_world = view_state_.view_from_world = p_list_->draw_cam.view_matrix();
            shrd_data.clip_from_view = p_list_->draw_cam.proj_matrix();

            shrd_data.taa_info[0] = p_list_->draw_cam.px_offset()[0];
            shrd_data.taa_info[1] = p_list_->draw_cam.px_offset()[1];
            memcpy(&shrd_data.taa_info[2], &view_state_.frame_index, sizeof(float));
            shrd_data.taa_info[3] =
                std::tan(0.5f * p_list_->draw_cam.angle() * Ren::Pi<float>() / 180.0f) / float(view_state_.act_res[1]);

            { // Ray Tracing Gems II, Listing 49-1
                const Ren::Plane &l = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Left);
                const Ren::Plane &r = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Right);
                const Ren::Plane &b = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Bottom);
                const Ren::Plane &t = p_list_->draw_cam.frustum_plane_vs(Ren::eCamPlane::Top);

                const float x0 = l.n[2] / l.n[0];
                const float x1 = r.n[2] / r.n[0];
                const float y0 = b.n[2] / b.n[1];
                const float y1 = t.n[2] / t.n[1];

                // View space position from screen space uv [0, 1]
                //  ray.xy = (frustum_info.zw * uv + frustum_info.xy) * mix(zDistanceNeg, -1.0, bIsOrtho)
                //  ray.z = 1.0 * zDistanceNeg

                shrd_data.frustum_info[0] = -x0;
                shrd_data.frustum_info[1] = -y0;
                shrd_data.frustum_info[2] = x0 - x1;
                shrd_data.frustum_info[3] = y0 - y1;

                view_state_.frustum_info = shrd_data.frustum_info;

                /*auto ReconstructViewPosition = [](const Ren::Vec2f uv, const Ren::Vec4f &cam_frustum,
                                                  const float view_z, const float is_ortho) {
                    Ren::Vec3f p;
                    p[0] = uv[0] * cam_frustum[2] + cam_frustum[0];
                    p[1] = uv[1] * cam_frustum[3] + cam_frustum[1];

                    p[0] *= view_z * (1.0f - std::abs(is_ortho)) + is_ortho;
                    p[1] *= view_z * (1.0f - std::abs(is_ortho)) + is_ortho;
                    p[2] = view_z;

                    return p;
                };*/
            }

            shrd_data.clip_from_view[2][0] += p_list_->draw_cam.px_offset()[0];
            shrd_data.clip_from_view[2][1] += p_list_->draw_cam.px_offset()[1];

            shrd_data.clip_from_world = view_state_.clip_from_world =
                (shrd_data.clip_from_view * shrd_data.view_from_world);
            Ren::Mat4f view_matrix_no_translation = shrd_data.view_from_world;
            view_matrix_no_translation[3][0] = view_matrix_no_translation[3][1] = view_matrix_no_translation[3][2] = 0;

            shrd_data.prev_view_from_world = view_state_.prev_view_from_world;
            shrd_data.prev_clip_from_world = view_state_.prev_clip_from_world;
            shrd_data.world_from_view = Inverse(shrd_data.view_from_world);
            shrd_data.view_from_clip = Inverse(shrd_data.clip_from_view);
            shrd_data.world_from_clip = Inverse(shrd_data.clip_from_world);
            // delta matrix between current and previous frame
            shrd_data.delta_matrix =
                view_state_.prev_clip_from_view * (view_state_.down_buf_view_from_world * shrd_data.world_from_view);
            shrd_data.rt_clip_from_world = p_list_->ext_cam.proj_matrix() * p_list_->ext_cam.view_matrix();

            if (p_list_->shadow_regions.count) {
                assert(p_list_->shadow_regions.count <= MAX_SHADOWMAPS_TOTAL);
                memcpy(&shrd_data.shadowmap_regions[0], &p_list_->shadow_regions.data[0],
                       sizeof(ShadowMapRegion) * p_list_->shadow_regions.count);
            }

            const float tan_angle = tanf(p_list_->env.sun_angle * Ren::Pi<float>() / 360.0f);
            shrd_data.sun_dir =
                Ren::Vec4f{-p_list_->env.sun_dir[0], -p_list_->env.sun_dir[1], -p_list_->env.sun_dir[2], tan_angle};
            shrd_data.sun_col = shrd_data.sun_col_point =
                Ren::Vec4f{p_list_->env.sun_col[0], p_list_->env.sun_col[1], p_list_->env.sun_col[2], 0.0f};
            if (p_list_->env.sun_angle != 0.0f) {
                const float radius = tan_angle;
                const float mul = 1.0f / (Ren::Pi<float>() * radius * radius);
                shrd_data.sun_col *= mul;
            }
            const float cos_theta = 1.0f / sqrtf(1.0f + tan_angle * tan_angle);
            shrd_data.sun_col[3] = shrd_data.sun_col_point[3] = cos_theta;
            shrd_data.env_col = Ren::Vec4f{p_list_->env.env_col[0], p_list_->env.env_col[1], p_list_->env.env_col[2],
                                           p_list_->env.env_map_rot};

            // actual resolution and full resolution
            shrd_data.res_and_fres = Ren::Vec4f{float(view_state_.act_res[0]), float(view_state_.act_res[1]),
                                                float(view_state_.scr_res[0]), float(view_state_.scr_res[1])};
            shrd_data.ires_and_ifres = Ren::Vec4i{view_state_.act_res[0], view_state_.act_res[1],
                                                  view_state_.scr_res[0], view_state_.scr_res[1]};
            { // main cam
                const float near = p_list_->draw_cam.near(), far = p_list_->draw_cam.far();
                const float time_s = 0.001f * float(Sys::GetTimeMs());
                const float transparent_near = near;
                const float transparent_far = 16.0f;
                const int transparent_mode = 0;

                shrd_data.transp_params_and_time =
                    Ren::Vec4f{std::log(transparent_near), std::log(transparent_far) - std::log(transparent_near),
                               float(transparent_mode), time_s};
                shrd_data.clip_info = Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};
                view_state_.clip_info = shrd_data.clip_info;
            }
            { // rt cam
                const float near = p_list_->ext_cam.near(), far = p_list_->ext_cam.far();
                shrd_data.rt_clip_info = Ren::Vec4f{near * far, near, far, std::log2(1.0f + far / near)};
            }

            { // 2 rotators for GI blur (perpendicular to each other)
                float _unused;
                const float rand_angle =
                    std::modf(float(view_state_.frame_index) * RendererInternal::GoldenRatio, &_unused) * 2.0f *
                    Ren::Pi<float>();
                const float ca = std::cos(rand_angle), sa = std::sin(rand_angle);
                view_state_.rand_rotators[0] = Ren::Vec4f{-sa, ca, -ca, sa};
                view_state_.rand_rotators[1] = Ren::Vec4f{ca, sa, -sa, ca};
            }

            { // random rotator used by GI probes
                const int sample_index =
                    view_state_.frame_index / (settings.gi_cache_update_mode == eGICacheUpdateMode::Partial
                                                   ? 8 * PROBE_VOLUMES_COUNT
                                                   : PROBE_VOLUMES_COUNT);

                float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
                if ((sample_index % 2) == 1) {
                    yaw = 0.3098f;
                    pitch = 0.5f;
                    roll = 0.5f;
                }

                auto hash = [](uint32_t x) {
                    // finalizer from murmurhash3
                    x ^= x >> 16;
                    x *= 0x85ebca6bu;
                    x ^= x >> 13;
                    x *= 0xc2b2ae35u;
                    x ^= x >> 16;
                    return x;
                };

                // yaw = rand_.GetNormalizedFloat();
                // pitch = rand_.GetNormalizedFloat();
                // yaw = rand_.GetNormalizedFloat();

                yaw *= 2.0f * Ren::Pi<float>();
                pitch *= 2.0f * Ren::Pi<float>();
                roll *= 2.0f * Ren::Pi<float>();

                view_state_.probe_ray_rotator = Ren::ToQuat(yaw, pitch, roll);
                view_state_.probe_ray_hash = hash(sample_index % 2);
            }

            const Ren::Vec3f &cam_pos = p_list_->draw_cam.world_position();
            shrd_data.prev_cam_pos =
                Ren::Vec4f{view_state_.prev_cam_pos[0], view_state_.prev_cam_pos[1], view_state_.prev_cam_pos[2], 0.0f};
            shrd_data.cam_pos_and_exp = Ren::Vec4f{cam_pos[0], cam_pos[1], cam_pos[2], view_state_.pre_exposure};
            shrd_data.wind_scroll =
                Ren::Vec4f{p_list_->env.curr_wind_scroll_lf[0], p_list_->env.curr_wind_scroll_lf[1],
                           p_list_->env.curr_wind_scroll_hf[0], p_list_->env.curr_wind_scroll_hf[1]};
            shrd_data.wind_scroll_prev =
                Ren::Vec4f{p_list_->env.prev_wind_scroll_lf[0], p_list_->env.prev_wind_scroll_lf[1],
                           p_list_->env.prev_wind_scroll_hf[0], p_list_->env.prev_wind_scroll_hf[1]};

            shrd_data.item_counts =
                Ren::Vec4u{uint32_t(p_list_->lights.size()), uint32_t(p_list_->decals.size()), 0, 0};
            float env_mip_count = 0.0f;
            if (p_list_->env.env_map) {
                env_mip_count = float(p_list_->env.env_map->params.mip_count);
            }
            shrd_data.ambient_hack = Ren::Vec4f{0.0f, 0.0f, 0.0f, env_mip_count};

            for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
                shrd_data.probe_volumes[i].origin =
                    Ren::Vec4f{persistent_data.probe_volumes[i].origin[0], persistent_data.probe_volumes[i].origin[1],
                               persistent_data.probe_volumes[i].origin[2], 0.0f};
                shrd_data.probe_volumes[i].spacing =
                    Ren::Vec4f{persistent_data.probe_volumes[i].spacing[0], persistent_data.probe_volumes[i].spacing[1],
                               persistent_data.probe_volumes[i].spacing[2], 0.0f};
                shrd_data.probe_volumes[i].scroll =
                    Ren::Vec4i{persistent_data.probe_volumes[i].scroll[0], persistent_data.probe_volumes[i].scroll[1],
                               persistent_data.probe_volumes[i].scroll[2], 0.0f};
            }

            memcpy(&shrd_data.probes[0], p_list_->probes.data(), sizeof(ProbeItem) * p_list_->probes.size());
            memcpy(&shrd_data.ellipsoids[0], p_list_->ellipsoids.data(),
                   sizeof(EllipsItem) * p_list_->ellipsoids.size());

            const int portals_count = std::min(int(p_list_->portals.size()), MAX_PORTALS_TOTAL);
            memcpy(&shrd_data.portals[0], p_list_->portals.data(), portals_count * sizeof(uint32_t));
            if (portals_count < MAX_PORTALS_TOTAL) {
                shrd_data.portals[portals_count] = 0xffffffff;
            }

            memcpy(&shrd_data.atmosphere, &p_list_->env.atmosphere, sizeof(AtmosphereParams));
            static_assert(sizeof(Eng::AtmosphereParams) == sizeof(Types::AtmosphereParams));

            Ren::UpdateBuffer(*shared_data_buf.ref, 0, sizeof(SharedDataBlock), &shrd_data,
                              *p_list_->shared_data_stage_buf, ctx.backend_frame() * SharedDataBlockSize,
                              SharedDataBlockSize, ctx.current_cmd_buf());
        }

        atomic_cnt_buf.ref->Fill(0, sizeof(uint32_t), 0, ctx.current_cmd_buf());
    });
}

void Eng::Renderer::AddLightBuffersUpdatePass(CommonBuffers &common_buffers) {
    auto &update_light_bufs = fg_builder_.AddNode("UPDATE LBUFFERS");

    { // create cells buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        common_buffers.cells = update_light_bufs.AddTransferOutput("Cells Buffer", desc);
    }
    { // create RT cells buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        common_buffers.rt_cells = update_light_bufs.AddTransferOutput("RT Cells Buffer", desc);
    }
    { // create lights buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        common_buffers.lights = update_light_bufs.AddTransferOutput("Lights Buffer", desc);
    }
    { // create decals buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        common_buffers.decals = update_light_bufs.AddTransferOutput("Decals Buffer", desc);
    }
    { // create items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        common_buffers.items = update_light_bufs.AddTransferOutput("Items Buffer", desc);
    }
    { // create RT items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        common_buffers.rt_items = update_light_bufs.AddTransferOutput("RT Items Buffer", desc);
    }

    update_light_bufs.set_execute_cb([this, &common_buffers](FgBuilder &builder) {
        Ren::Context &ctx = builder.ctx();
        FgAllocBuf &cells_buf = builder.GetWriteBuffer(common_buffers.cells);
        FgAllocBuf &rt_cells_buf = builder.GetWriteBuffer(common_buffers.rt_cells);
        FgAllocBuf &lights_buf = builder.GetWriteBuffer(common_buffers.lights);
        FgAllocBuf &decals_buf = builder.GetWriteBuffer(common_buffers.decals);
        FgAllocBuf &items_buf = builder.GetWriteBuffer(common_buffers.items);
        FgAllocBuf &rt_items_buf = builder.GetWriteBuffer(common_buffers.rt_items);

        if (!cells_buf.tbos[0]) {
            cells_buf.tbos[0] =
                ctx.CreateTexture1D("Cells TBO", cells_buf.ref, Ren::eTexFormat::RG32UI, 0, CellsBufChunkSize);
        }

        Ren::UpdateBuffer(*cells_buf.ref, 0, p_list_->cells.count * sizeof(CellData), p_list_->cells.data,
                          *p_list_->cells_stage_buf, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize,
                          ctx.current_cmd_buf());

        if (!rt_cells_buf.tbos[0]) {
            rt_cells_buf.tbos[0] =
                ctx.CreateTexture1D("RT Cells TBO", rt_cells_buf.ref, Ren::eTexFormat::RG32UI, 0, CellsBufChunkSize);
        }

        Ren::UpdateBuffer(*rt_cells_buf.ref, 0, p_list_->rt_cells.count * sizeof(CellData), p_list_->rt_cells.data,
                          *p_list_->rt_cells_stage_buf, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize,
                          ctx.current_cmd_buf());

        if (!lights_buf.tbos[0]) {
            lights_buf.tbos[0] =
                ctx.CreateTexture1D("Lights TBO", lights_buf.ref, Ren::eTexFormat::RGBA32F, 0, LightsBufChunkSize);
        }

        Ren::UpdateBuffer(*lights_buf.ref, 0, uint32_t(p_list_->lights.size() * sizeof(LightItem)),
                          p_list_->lights.data(), *p_list_->lights_stage_buf, ctx.backend_frame() * LightsBufChunkSize,
                          LightsBufChunkSize, ctx.current_cmd_buf());

        if (!decals_buf.tbos[0]) {
            decals_buf.tbos[0] =
                ctx.CreateTexture1D("Decals TBO", decals_buf.ref, Ren::eTexFormat::RGBA32F, 0, DecalsBufChunkSize);
        }

        Ren::UpdateBuffer(*decals_buf.ref, 0, uint32_t(p_list_->decals.size() * sizeof(DecalItem)),
                          p_list_->decals.data(), *p_list_->decals_stage_buf, ctx.backend_frame() * DecalsBufChunkSize,
                          DecalsBufChunkSize, ctx.current_cmd_buf());

        if (!items_buf.tbos[0]) {
            items_buf.tbos[0] =
                ctx.CreateTexture1D("Items TBO", items_buf.ref, Ren::eTexFormat::RG32UI, 0, ItemsBufChunkSize);
        }

        if (p_list_->items.count) {
            Ren::UpdateBuffer(*items_buf.ref, 0, p_list_->items.count * sizeof(ItemData), p_list_->items.data,
                              *p_list_->items_stage_buf, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize,
                              ctx.current_cmd_buf());
        } else {
            const ItemData dummy = {};
            Ren::UpdateBuffer(*items_buf.ref, 0, sizeof(ItemData), &dummy, *p_list_->items_stage_buf,
                              ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
        }

        if (!rt_items_buf.tbos[0]) {
            rt_items_buf.tbos[0] =
                ctx.CreateTexture1D("RT Items TBO", rt_items_buf.ref, Ren::eTexFormat::RG32UI, 0, ItemsBufChunkSize);
        }

        if (p_list_->rt_items.count) {
            Ren::UpdateBuffer(*rt_items_buf.ref, 0, p_list_->rt_items.count * sizeof(ItemData), p_list_->rt_items.data,
                              *p_list_->rt_items_stage_buf, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize,
                              ctx.current_cmd_buf());
        } else {
            const ItemData dummy = {};
            Ren::UpdateBuffer(*rt_items_buf.ref, 0, sizeof(ItemData), &dummy, *p_list_->rt_items_stage_buf,
                              ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
        }
    });
}

void Eng::Renderer::InitSkyResources() {
    if (p_list_->env.env_map_name != "physical_sky") {
        // release resources
        sky_transmittance_lut_ = {};
        sky_multiscatter_lut_ = {};
        sky_moon_tex_ = {};
        sky_weather_tex_ = {};
        sky_cirrus_tex_ = {};
        sky_curl_tex_ = {};
        sky_noise3d_tex_ = {};
    } else {
        if (!sky_transmittance_lut_) {
            const std::vector<Ren::Vec4f> transmittance_lut = Generate_SkyTransmittanceLUT(p_list_->env.atmosphere);
            { // Init transmittance LUT
                Ren::Tex2DParams p;
                p.w = SKY_TRANSMITTANCE_LUT_W;
                p.h = SKY_TRANSMITTANCE_LUT_H;
                p.format = Ren::eTexFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                Ren::eTexLoadStatus status;
                sky_transmittance_lut_ =
                    ctx_.LoadTexture2D("Sky Transmittance LUT", p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                Ren::Buffer stage_buf("Temp Stage Buf", ctx_.api_ctx(), Ren::eBufType::Upload,
                                      4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
                { // init stage buf
                    uint8_t *mapped_ptr = stage_buf.Map();
                    memcpy(mapped_ptr, transmittance_lut.data(),
                           4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
                    stage_buf.Unmap();
                }

                sky_transmittance_lut_->SetSubImage(
                    0, 0, 0, SKY_TRANSMITTANCE_LUT_W, SKY_TRANSMITTANCE_LUT_H, Ren::eTexFormat::RGBA32F, stage_buf,
                    ctx_.current_cmd_buf(), 0, 4 * SKY_TRANSMITTANCE_LUT_W * SKY_TRANSMITTANCE_LUT_H * sizeof(float));
            }
            { // Init multiscatter LUT
                Ren::Tex2DParams p;
                p.w = p.h = SKY_MULTISCATTER_LUT_RES;
                p.format = Ren::eTexFormat::RGBA32F;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                Ren::eTexLoadStatus status;
                sky_multiscatter_lut_ =
                    ctx_.LoadTexture2D("Sky Multiscatter LUT", p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                const std::vector<Ren::Vec4f> multiscatter_lut =
                    Generate_SkyMultiscatterLUT(p_list_->env.atmosphere, transmittance_lut);

                Ren::Buffer stage_buf("Temp Stage Buf", ctx_.api_ctx(), Ren::eBufType::Upload,
                                      4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
                { // init stage buf
                    uint8_t *mapped_ptr = stage_buf.Map();
                    memcpy(mapped_ptr, multiscatter_lut.data(),
                           4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
                    stage_buf.Unmap();
                }

                sky_multiscatter_lut_->SetSubImage(
                    0, 0, 0, SKY_MULTISCATTER_LUT_RES, SKY_MULTISCATTER_LUT_RES, Ren::eTexFormat::RGBA32F, stage_buf,
                    ctx_.current_cmd_buf(), 0, 4 * SKY_MULTISCATTER_LUT_RES * SKY_MULTISCATTER_LUT_RES * sizeof(float));
            }
            { // Init Moon texture
                Sys::AssetFile moon_tex("assets_pc/textures/internal/moon_diff.dds");
                std::vector<uint8_t> data(moon_tex.size());
                moon_tex.Read((char *)&data[0], moon_tex.size());

                Ren::Tex2DParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_moon_tex_ = ctx_.LoadTexture2D(moon_tex.name(), data, p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init Weather texture
                Sys::AssetFile weather_tex("assets_pc/textures/internal/weather.dds");
                std::vector<uint8_t> data(weather_tex.size());
                weather_tex.Read((char *)&data[0], weather_tex.size());

                Ren::Tex2DParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_weather_tex_ = ctx_.LoadTexture2D(weather_tex.name(), data, p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init Cirrus texture
                Sys::AssetFile cirrus_tex("assets_pc/textures/internal/cirrus.dds");
                std::vector<uint8_t> data(cirrus_tex.size());
                cirrus_tex.Read((char *)&data[0], cirrus_tex.size());

                Ren::Tex2DParams p;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_cirrus_tex_ = ctx_.LoadTexture2D(cirrus_tex.name(), data, p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init Curl texture
                Sys::AssetFile curl_tex("assets_pc/textures/internal/curl.dds");
                std::vector<uint8_t> data(curl_tex.size());
                curl_tex.Read((char *)&data[0], curl_tex.size());

                Ren::Tex2DParams p;
                p.flags = Ren::eTexFlags::SRGB;
                p.usage = Ren::Bitmask(Ren::eTexUsage::Transfer) | Ren::eTexUsage::Sampled;
                p.sampling.filter = Ren::eTexFilter::Bilinear;
                p.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_curl_tex_ = ctx_.LoadTexture2D(curl_tex.name(), data, p, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedFromData);
            }
            { // Init 3d noise texture
                Sys::AssetFile noise_tex("assets_pc/textures/internal/3dnoise.dds");
                std::vector<uint8_t> data(noise_tex.size());
                noise_tex.Read((char *)&data[0], noise_tex.size());

                Ren::DDSHeader header = {};
                memcpy(&header, &data[0], sizeof(Ren::DDSHeader));

                const uint32_t data_len = header.dwWidth * header.dwHeight * header.dwDepth;

                Ren::Tex3DParams params;
                params.w = header.dwWidth;
                params.h = header.dwHeight;
                params.d = header.dwDepth;
                params.format = Ren::eTexFormat::R8;
                params.usage = Ren::Bitmask(Ren::eTexUsage::Sampled) | Ren::eTexUsage::Transfer;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::Repeat;

                Ren::eTexLoadStatus status;
                sky_noise3d_tex_ = ctx_.LoadTexture3D("Noise 3d Tex", params, ctx_.default_mem_allocs(), &status);
                assert(status == Ren::eTexLoadStatus::CreatedDefault);

                Ren::Buffer stage_buf = Ren::Buffer("Temp stage buf", ctx_.api_ctx(), Ren::eBufType::Upload, data_len);
                uint8_t *mapped_ptr = stage_buf.Map();
                memcpy(mapped_ptr, &data[0] + sizeof(Ren::DDSHeader), data_len);
                stage_buf.Unmap();

                sky_noise3d_tex_->SetSubImage(0, 0, 0, int(header.dwWidth), int(header.dwHeight), int(header.dwDepth),
                                              Ren::eTexFormat::R8, stage_buf, ctx_.current_cmd_buf(), 0, int(data_len));
            }
        }
    }
}

void Eng::Renderer::AddSkydomePass(const CommonBuffers &common_buffers, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    // TODO: Remove this condition
    if (!p_list_->env.env_map) {
        return;
    }

    if (p_list_->env.env_map_name == "physical_sky") {
        auto &skydome_cube = fg_builder_.AddNode("SKYDOME CUBE");

        auto *data = skydome_cube.AllocNodeData<ExSkydomeCube::Args>();

        data->shared_data =
            skydome_cube.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

        data->transmittance_lut = skydome_cube.AddTextureInput(sky_transmittance_lut_, Stg::FragmentShader);
        data->multiscatter_lut = skydome_cube.AddTextureInput(sky_multiscatter_lut_, Stg::FragmentShader);
        data->moon_tex = skydome_cube.AddTextureInput(sky_moon_tex_, Stg::FragmentShader);
        data->weather_tex = skydome_cube.AddTextureInput(sky_weather_tex_, Stg::FragmentShader);
        data->cirrus_tex = skydome_cube.AddTextureInput(sky_cirrus_tex_, Stg::FragmentShader);
        data->curl_tex = skydome_cube.AddTextureInput(sky_curl_tex_, Stg::FragmentShader);
        data->noise3d_tex = skydome_cube.AddTextureInput(sky_noise3d_tex_.get(), Stg::FragmentShader);
        frame_textures.envmap = data->color_tex = skydome_cube.AddColorOutput(p_list_->env.env_map);

        ex_skydome_cube_.Setup(&view_state_, data);
        skydome_cube.set_executor(&ex_skydome_cube_);
    }

    FgResRef sky_temp;
    { // Main pass
        auto &skymap = fg_builder_.AddNode("SKYDOME");

        auto *data = skymap.AllocNodeData<ExSkydomeScreen::Args>();
        data->shared_data =
            skymap.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);
        if (p_list_->env.env_map_name != "physical_sky") {
            frame_textures.envmap = data->env_tex = skymap.AddTextureInput(p_list_->env.env_map, Stg::FragmentShader);
            frame_textures.color = data->color_tex = skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
            frame_textures.depth = data->depth_tex = skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
        } else {
            data->sky_quality = settings.sky_quality;
            frame_textures.envmap = data->env_tex = skymap.AddTextureInput(frame_textures.envmap, Stg::FragmentShader);
            data->phys.transmittance_lut = skymap.AddTextureInput(sky_transmittance_lut_, Stg::FragmentShader);
            data->phys.multiscatter_lut = skymap.AddTextureInput(sky_multiscatter_lut_, Stg::FragmentShader);
            data->phys.moon_tex = skymap.AddTextureInput(sky_moon_tex_, Stg::FragmentShader);
            data->phys.weather_tex = skymap.AddTextureInput(sky_weather_tex_, Stg::FragmentShader);
            data->phys.cirrus_tex = skymap.AddTextureInput(sky_cirrus_tex_, Stg::FragmentShader);
            data->phys.curl_tex = skymap.AddTextureInput(sky_curl_tex_, Stg::FragmentShader);
            data->phys.noise3d_tex = skymap.AddTextureInput(sky_noise3d_tex_.get(), Stg::FragmentShader);

            if (settings.sky_quality == eSkyQuality::High) {
                frame_textures.depth = data->depth_tex =
                    skymap.AddTextureInput(frame_textures.depth, Stg::FragmentShader);

                Ren::Tex2DParams params;
                params.w = (view_state_.scr_res[0] + 3) / 4;
                params.h = (view_state_.scr_res[1] + 3) / 4;
                params.format = Ren::eTexFormat::RGBA16F;
                params.sampling.filter = Ren::eTexFilter::Bilinear;
                params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

                sky_temp = data->color_tex = skymap.AddColorOutput("SKY TEMP", params);
            } else {
                frame_textures.color = data->color_tex =
                    skymap.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
                frame_textures.depth = data->depth_tex =
                    skymap.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);
            }
        }

        ex_skydome_.Setup(&view_state_, data);
        skymap.set_executor(&ex_skydome_);
    }

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    if (settings.sky_quality == eSkyQuality::High) {
        auto &sky_upsample = fg_builder_.AddNode("SKY UPSAMPLE");

        struct PassData {
            FgResRef shared_data;
            FgResRef depth_tex;
            FgResRef env_map;
            FgResRef sky_temp_tex;
            FgResRef sky_hist_tex;
            FgResRef output_tex;
            FgResRef output_hist_tex;
        };

        auto *data = sky_upsample.AllocNodeData<PassData>();
        data->shared_data = sky_upsample.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        frame_textures.depth = data->depth_tex = sky_upsample.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        frame_textures.envmap = data->env_map = sky_upsample.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);
        data->sky_temp_tex = sky_upsample.AddTextureInput(sky_temp, Stg::ComputeShader);

        FgResRef sky_upsampled;
        {
            Ren::Tex2DParams params;
            params.w = view_state_.scr_res[0];
            params.h = view_state_.scr_res[1];
            params.format = Ren::eTexFormat::RGBA16F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            sky_upsampled = data->output_hist_tex =
                sky_upsample.AddStorageImageOutput("SKY HIST", params, Stg::ComputeShader);
        }

        data->sky_hist_tex = sky_upsample.AddHistoryTextureInput(sky_upsampled, Stg::ComputeShader);
        frame_textures.color = data->output_tex =
            sky_upsample.AddStorageImageOutput(MAIN_COLOR_TEX, frame_textures.color_params, Stg::ComputeShader);

        sky_upsample.set_execute_cb([data, this](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
            FgAllocTex &env_map_tex = builder.GetReadTexture(data->env_map);
            FgAllocTex &sky_temp_tex = builder.GetReadTexture(data->sky_temp_tex);
            FgAllocTex &sky_hist_tex = builder.GetReadTexture(data->sky_hist_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);
            FgAllocTex &output_hist_tex = builder.GetWriteTexture(data->output_hist_tex);

            const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                             {Trg::Tex2DSampled, Skydome::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                             {Trg::Tex2DSampled, Skydome::ENV_TEX_SLOT, *env_map_tex.ref},
                                             {Trg::Tex2DSampled, Skydome::SKY_TEX_SLOT, *sky_temp_tex.ref},
                                             {Trg::Tex2DSampled, Skydome::SKY_HIST_TEX_SLOT, *sky_hist_tex.ref},
                                             {Trg::Image2D, Skydome::OUT_IMG_SLOT, *output_tex.ref},
                                             {Trg::Image2D, Skydome::OUT_HIST_IMG_SLOT, *output_hist_tex.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (view_state_.act_res[0] + Skydome::LOCAL_GROUP_SIZE_X - 1u) / Skydome::LOCAL_GROUP_SIZE_X,
                (view_state_.act_res[1] + Skydome::LOCAL_GROUP_SIZE_Y - 1u) / Skydome::LOCAL_GROUP_SIZE_Y, 1u};

            Skydome::Params2 uniform_params;
            uniform_params.img_size = view_state_.scr_res;
            uniform_params.sample_coord = ExSkydomeScreen::sample_pos(view_state_.frame_index);
            uniform_params.hist_weight = (view_state_.pre_exposure / view_state_.prev_pre_exposure);

            DispatchCompute(*pi_sky_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
}

void Eng::Renderer::AddSunColorUpdatePass(CommonBuffers &common_buffers) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    if (p_list_->env.env_map_name != "physical_sky") {
        return;
    }

    FgResRef output;
    { // Sample sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR SAMPLE");

        struct PassData {
            FgResRef shared_data;
            FgResRef transmittance_lut;
            FgResRef multiscatter_lut;
            FgResRef moon_tex;
            FgResRef weather_tex;
            FgResRef cirrus_tex;
            FgResRef noise3d_tex;
            FgResRef output_buf;
        };

        auto *data = sun_color.AllocNodeData<PassData>();

        data->shared_data = sun_color.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);
        data->transmittance_lut = sun_color.AddTextureInput(sky_transmittance_lut_, Stg::ComputeShader);
        data->multiscatter_lut = sun_color.AddTextureInput(sky_multiscatter_lut_, Stg::ComputeShader);
        data->moon_tex = sun_color.AddTextureInput(sky_moon_tex_, Stg::ComputeShader);
        data->weather_tex = sun_color.AddTextureInput(sky_weather_tex_, Stg::ComputeShader);
        data->cirrus_tex = sun_color.AddTextureInput(sky_cirrus_tex_, Stg::ComputeShader);
        data->noise3d_tex = sun_color.AddTextureInput(sky_noise3d_tex_.get(), Stg::ComputeShader);

        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Storage;
        desc.size = 8 * sizeof(float);
        output = data->output_buf = sun_color.AddStorageOutput("Sun Brightness Result", desc, Stg::ComputeShader);

        sun_color.set_execute_cb([data, this](FgBuilder &builder) {
            FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(data->shared_data);
            FgAllocTex &transmittance_lut = builder.GetReadTexture(data->transmittance_lut);
            FgAllocTex &multiscatter_lut = builder.GetReadTexture(data->multiscatter_lut);
            FgAllocTex &moon_tex = builder.GetReadTexture(data->moon_tex);
            FgAllocTex &weather_tex = builder.GetReadTexture(data->weather_tex);
            FgAllocTex &cirrus_tex = builder.GetReadTexture(data->cirrus_tex);
            FgAllocTex &noise3d_tex = builder.GetReadTexture(data->noise3d_tex);
            FgAllocBuf &output_buf = builder.GetWriteBuffer(data->output_buf);

            const Ren::Binding bindings[] = {
                {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                {Trg::Tex2DSampled, SunBrightness::TRANSMITTANCE_LUT_SLOT, *transmittance_lut.ref},
                {Trg::Tex2DSampled, SunBrightness::MULTISCATTER_LUT_SLOT, *multiscatter_lut.ref},
                {Trg::Tex2DSampled, SunBrightness::MOON_TEX_SLOT, *moon_tex.ref},
                {Trg::Tex2DSampled, SunBrightness::WEATHER_TEX_SLOT, *weather_tex.ref},
                {Trg::Tex2DSampled, SunBrightness::CIRRUS_TEX_SLOT, *cirrus_tex.ref},
                {Trg::Tex3DSampled, SunBrightness::NOISE3D_TEX_SLOT,
                 *std::get<const Ren::Texture3D *>(noise3d_tex._ref)},
                {Trg::SBufRW, SunBrightness::OUT_BUF_SLOT, *output_buf.ref}};

            DispatchCompute(*pi_sun_brightness_, Ren::Vec3u{1u, 1u, 1u}, bindings, nullptr, 0,
                            builder.ctx().default_descr_alloc(), builder.ctx().log());
        });
    }
    { // Update sun color
        auto &sun_color = fg_builder_.AddNode("SUN COLOR UPDATE");

        struct PassData {
            FgResRef sample_buf;
            FgResRef shared_data;
        };

        auto *data = sun_color.AllocNodeData<PassData>();

        data->sample_buf = sun_color.AddTransferInput(output);
        common_buffers.shared_data = data->shared_data = sun_color.AddTransferOutput(common_buffers.shared_data);

        sun_color.set_execute_cb([data](FgBuilder &builder) {
            FgAllocBuf &sample_buf = builder.GetReadBuffer(data->sample_buf);
            FgAllocBuf &unif_sh_data_buf = builder.GetWriteBuffer(data->shared_data);

            CopyBufferToBuffer(*sample_buf.ref, 0, *unif_sh_data_buf.ref, offsetof(SharedDataBlock, sun_col),
                               3 * sizeof(float), builder.ctx().current_cmd_buf());
            CopyBufferToBuffer(*sample_buf.ref, 4 * sizeof(float), *unif_sh_data_buf.ref,
                               offsetof(SharedDataBlock, sun_col_point_sh), 3 * sizeof(float),
                               builder.ctx().current_cmd_buf());
        });
    }
}

void Eng::Renderer::AddGBufferFillPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                       const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;

    auto &gbuf_fill = fg_builder_.AddNode("GBUFFER FILL");
    const FgResRef vtx_buf1 = gbuf_fill.AddVertexBufferInput(persistent_data.vertex_buf1);
    const FgResRef vtx_buf2 = gbuf_fill.AddVertexBufferInput(persistent_data.vertex_buf2);
    const FgResRef ndx_buf = gbuf_fill.AddIndexBufferInput(persistent_data.indices_buf);

    const FgResRef materials_buf = gbuf_fill.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
    const FgResRef textures_buf = gbuf_fill.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
    const FgResRef textures_buf = {};
#endif

    const FgResRef noise_tex = gbuf_fill.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
    const FgResRef dummy_white = gbuf_fill.AddTextureInput(dummy_white_, Stg::FragmentShader);
    const FgResRef dummy_black = gbuf_fill.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = gbuf_fill.AddStorageReadonlyInput(
        persistent_data.instance_buf, persistent_data.instance_buf_tbo, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shared_data_buf =
        gbuf_fill.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

    const FgResRef cells_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgResRef items_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgResRef decals_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    frame_textures.albedo = gbuf_fill.AddColorOutput(MAIN_ALBEDO_TEX, frame_textures.albedo_params);
    frame_textures.normal = gbuf_fill.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = gbuf_fill.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = gbuf_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    ex_gbuffer_fill_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless,
                           noise_tex, dummy_white, dummy_black, instances_buf, instances_indices_buf, shared_data_buf,
                           cells_buf, items_buf, decals_buf, frame_textures.albedo, frame_textures.normal,
                           frame_textures.specular, frame_textures.depth);
    gbuf_fill.set_executor(&ex_gbuffer_fill_);
}

void Eng::Renderer::AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                         const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;

    auto &opaque = fg_builder_.AddNode("OPAQUE");
    const FgResRef vtx_buf1 = opaque.AddVertexBufferInput(persistent_data.vertex_buf1);
    const FgResRef vtx_buf2 = opaque.AddVertexBufferInput(persistent_data.vertex_buf2);
    const FgResRef ndx_buf = opaque.AddIndexBufferInput(persistent_data.indices_buf);

    const FgResRef materials_buf = opaque.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
    const FgResRef textures_buf = opaque.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
    const FgResRef textures_buf = {};
#endif
    const FgResRef brdf_lut = opaque.AddTextureInput(brdf_lut_, Stg::FragmentShader);
    const FgResRef noise_tex = opaque.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
    const FgResRef cone_rt_lut = opaque.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

    const FgResRef dummy_black = opaque.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = opaque.AddStorageReadonlyInput(persistent_data.instance_buf,
                                                                  persistent_data.instance_buf_tbo, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        opaque.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shader_data_buf =
        opaque.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

    const FgResRef cells_buf = opaque.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgResRef items_buf = opaque.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgResRef lights_buf = opaque.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
    const FgResRef decals_buf = opaque.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    const FgResRef shadowmap_tex = opaque.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
    const FgResRef ssao_tex = opaque.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

    FgResRef lmap_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (p_list_->env.lm_indir_sh[i]) {
            lmap_tex[i] = opaque.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
        }
    }

    frame_textures.color = opaque.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.normal = opaque.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = opaque.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = opaque.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    ex_opaque_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
                     persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
                     instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf,
                     decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color, frame_textures.normal,
                     frame_textures.specular, frame_textures.depth);
    opaque.set_executor(&ex_opaque_);
}

void Eng::Renderer::AddForwardTransparentPass(const CommonBuffers &common_buffers,
                                              const PersistentGpuData &persistent_data,
                                              const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;

    auto &transparent = fg_builder_.AddNode("TRANSPARENT");
    const FgResRef vtx_buf1 = transparent.AddVertexBufferInput(persistent_data.vertex_buf1);
    const FgResRef vtx_buf2 = transparent.AddVertexBufferInput(persistent_data.vertex_buf2);
    const FgResRef ndx_buf = transparent.AddIndexBufferInput(persistent_data.indices_buf);

    const FgResRef materials_buf =
        transparent.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
    const FgResRef textures_buf = transparent.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
    const FgResRef textures_buf = {};
#endif
    const FgResRef brdf_lut = transparent.AddTextureInput(brdf_lut_, Stg::FragmentShader);
    const FgResRef noise_tex = transparent.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
    const FgResRef cone_rt_lut = transparent.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

    const FgResRef dummy_black = transparent.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = transparent.AddStorageReadonlyInput(
        persistent_data.instance_buf, persistent_data.instance_buf_tbo, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        transparent.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shader_data_buf =
        transparent.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

    const FgResRef cells_buf = transparent.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgResRef items_buf = transparent.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgResRef lights_buf = transparent.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
    const FgResRef decals_buf = transparent.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    const FgResRef shadowmap_tex = transparent.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
    const FgResRef ssao_tex = transparent.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

    FgResRef lmap_tex[4];
    for (int i = 0; i < 4; ++i) {
        if (p_list_->env.lm_indir_sh[i]) {
            lmap_tex[i] = transparent.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
        }
    }

    frame_textures.color = transparent.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.normal = transparent.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = transparent.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = transparent.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    ex_transparent_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
                          persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black,
                          instances_buf, instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf,
                          decals_buf, shadowmap_tex, ssao_tex, lmap_tex, frame_textures.color, frame_textures.normal,
                          frame_textures.specular, frame_textures.depth);
    transparent.set_executor(&ex_transparent_);
}

void Eng::Renderer::AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures,
                                           const bool enable_gi) {
    using Stg = Ren::eStageBits;
    using Trg = Ren::eBindTarget;

    auto &gbuf_shade = fg_builder_.AddNode("GBUFFER SHADE");

    struct PassData {
        FgResRef shared_data;
        FgResRef cells_buf, items_buf, lights_buf, decals_buf;
        FgResRef shadow_depth_tex, shadow_color_tex, ssao_tex, gi_tex, sun_shadow_tex;
        FgResRef depth_tex, albedo_tex, normal_tex, spec_tex;
        FgResRef ltc_luts_tex, env_tex;
        FgResRef output_tex;
    };

    auto *data = gbuf_shade.AllocNodeData<PassData>();
    data->shared_data = gbuf_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

    data->cells_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.cells, Stg::ComputeShader);
    data->items_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.items, Stg::ComputeShader);
    data->lights_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
    data->decals_buf = gbuf_shade.AddStorageReadonlyInput(common_buffers.decals, Stg::ComputeShader);

    data->shadow_depth_tex = gbuf_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
    data->shadow_color_tex = gbuf_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
    data->ssao_tex = gbuf_shade.AddTextureInput(frame_textures.ssao, Stg::ComputeShader);
    if (enable_gi) {
        data->gi_tex = gbuf_shade.AddTextureInput(frame_textures.gi, Stg::ComputeShader);
    } else {
        data->gi_tex = gbuf_shade.AddTextureInput(dummy_black_, Stg::ComputeShader);
    }
    data->sun_shadow_tex = gbuf_shade.AddTextureInput(frame_textures.sun_shadow, Stg::ComputeShader);

    data->depth_tex = gbuf_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
    data->albedo_tex = gbuf_shade.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
    data->normal_tex = gbuf_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
    data->spec_tex = gbuf_shade.AddTextureInput(frame_textures.specular, Stg::ComputeShader);

    data->ltc_luts_tex = gbuf_shade.AddTextureInput(ltc_luts_, Stg::ComputeShader);
    data->env_tex = gbuf_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);

    frame_textures.color = data->output_tex =
        gbuf_shade.AddStorageImageOutput(MAIN_COLOR_TEX, frame_textures.color_params, Stg::ComputeShader);

    gbuf_shade.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);
        FgAllocBuf &cells_buf = builder.GetReadBuffer(data->cells_buf);
        FgAllocBuf &items_buf = builder.GetReadBuffer(data->items_buf);
        FgAllocBuf &lights_buf = builder.GetReadBuffer(data->lights_buf);
        FgAllocBuf &decals_buf = builder.GetReadBuffer(data->decals_buf);

        FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        FgAllocTex &albedo_tex = builder.GetReadTexture(data->albedo_tex);
        FgAllocTex &normal_tex = builder.GetReadTexture(data->normal_tex);
        FgAllocTex &spec_tex = builder.GetReadTexture(data->spec_tex);

        FgAllocTex &shad_depth_tex = builder.GetReadTexture(data->shadow_depth_tex);
        FgAllocTex &shad_color_tex = builder.GetReadTexture(data->shadow_color_tex);
        FgAllocTex &ssao_tex = builder.GetReadTexture(data->ssao_tex);
        FgAllocTex &gi_tex = builder.GetReadTexture(data->gi_tex);
        FgAllocTex &sun_shadow_tex = builder.GetReadTexture(data->sun_shadow_tex);

        FgAllocTex &ltc_luts = builder.GetReadTexture(data->ltc_luts_tex);
        FgAllocTex &env_tex = builder.GetReadTexture(data->env_tex);

        FgAllocTex &out_color_tex = builder.GetWriteTexture(data->output_tex);

        const Ren::Binding bindings[] = {
            {Trg::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_shared_data_buf.ref},
            {Trg::UTBuf, GBufferShade::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
            {Trg::UTBuf, GBufferShade::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
            {Trg::UTBuf, GBufferShade::LIGHT_BUF_SLOT, *lights_buf.tbos[0]},
            {Trg::UTBuf, GBufferShade::DECAL_BUF_SLOT, *decals_buf.tbos[0]},
            {Trg::Tex2DSampled, GBufferShade::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Trg::Tex2DSampled, GBufferShade::DEPTH_LIN_TEX_SLOT, {*depth_tex.ref, *linear_sampler_, 1}},
            {Trg::Tex2DSampled, GBufferShade::ALBEDO_TEX_SLOT, *albedo_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::NORM_TEX_SLOT, *normal_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::SPEC_TEX_SLOT, *spec_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::SHADOW_DEPTH_TEX_SLOT, *shad_depth_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::SHADOW_DEPTH_VAL_TEX_SLOT, {*shad_depth_tex.ref, *nearest_sampler_}},
            {Trg::Tex2DSampled, GBufferShade::SHADOW_COLOR_TEX_SLOT, *shad_color_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::SSAO_TEX_SLOT, *ssao_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::GI_TEX_SLOT, *gi_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::SUN_SHADOW_TEX_SLOT, *sun_shadow_tex.ref},
            {Trg::Tex2DSampled, GBufferShade::LTC_LUTS_TEX_SLOT, *ltc_luts.ref},
            {Trg::Tex2DSampled, GBufferShade::ENV_TEX_SLOT, *env_tex.ref},
            {Trg::Image2D, GBufferShade::OUT_COLOR_IMG_SLOT, *out_color_tex.ref}};

        const Ren::Vec3u grp_count = Ren::Vec3u{
            (view_state_.act_res[0] + GBufferShade::LOCAL_GROUP_SIZE_X - 1u) / GBufferShade::LOCAL_GROUP_SIZE_X,
            (view_state_.act_res[1] + GBufferShade::LOCAL_GROUP_SIZE_Y - 1u) / GBufferShade::LOCAL_GROUP_SIZE_Y, 1u};

        GBufferShade::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u(view_state_.act_res[0], view_state_.act_res[1]);
        uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;

        DispatchCompute(*pi_gbuf_shade_[settings.enable_shadow_jitter], grp_count, bindings, &uniform_params,
                        sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.ctx().log());
    });
}

void Eng::Renderer::AddEmissivesPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                     const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStageBits;

    auto &emissive = fg_builder_.AddNode("EMISSIVE");
    const FgResRef vtx_buf1 = emissive.AddVertexBufferInput(persistent_data.vertex_buf1);
    const FgResRef vtx_buf2 = emissive.AddVertexBufferInput(persistent_data.vertex_buf2);
    const FgResRef ndx_buf = emissive.AddIndexBufferInput(persistent_data.indices_buf);

    const FgResRef materials_buf = emissive.AddStorageReadonlyInput(persistent_data.materials_buf, Stg::VertexShader);
#if defined(REN_GL_BACKEND)
    const FgResRef textures_buf = emissive.AddStorageReadonlyInput(bindless.textures_buf, Stg::VertexShader);
#else
    const FgResRef textures_buf = {};
#endif

    const FgResRef noise_tex = emissive.AddTextureInput(noise_tex_, Stg::VertexShader | Stg::FragmentShader);
    const FgResRef dummy_white = emissive.AddTextureInput(dummy_white_, Stg::FragmentShader);

    const FgResRef instances_buf = emissive.AddStorageReadonlyInput(
        persistent_data.instance_buf, persistent_data.instance_buf_tbo, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        emissive.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shared_data_buf =
        emissive.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);

    frame_textures.color = emissive.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.depth = emissive.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    ex_emissive_.Setup(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless,
                       noise_tex, dummy_white, instances_buf, instances_indices_buf, shared_data_buf,
                       frame_textures.color, frame_textures.depth);
    emissive.set_executor(&ex_emissive_);
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
            Ren::Tex2DParams params;
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

            { // prepare ao buffer
                const Ren::Binding bindings[] = {{Trg::Tex2DSampled, SSAO::DEPTH_TEX_SLOT, *down_depth_2x_tex.ref},
                                                 {Trg::Tex2DSampled, SSAO::RAND_TEX_SLOT, *rand_tex.ref}};

                SSAO::Params uniform_params;
                uniform_params.transform =
                    Ren::Vec4f{0.0f, 0.0f, view_state_.act_res[0] / 2, view_state_.act_res[1] / 2};
                uniform_params.resolution = Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};

                const Ren::RenderTarget render_targets[] = {
                    {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

                prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_ao_prog_, {}, render_targets, rast_state,
                                    builder.rast_state(), bindings, &uniform_params, sizeof(SSAO::Params), 0);
            }
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
            Ren::Tex2DParams params;
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
            Ren::Tex2DParams params;
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
            Ren::Tex2DParams params;
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
            Ren::Tex2DParams params;
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
                                             {Trg::Image2D, GTAO::OUT_IMG_SLOT, *output_tex.ref}};

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
            Ren::Tex2DParams params;
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
                                             {Trg::Image2D, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

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
            Ren::Tex2DParams params;
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
                                             {Trg::Image2D, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

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
            Ren::Tex2DParams params;
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
                                             {Trg::Image2D, GTAO::OUT_IMG_SLOT, *out_ao_tex.ref}};

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

void Eng::Renderer::AddFillStaticVelocityPass(const CommonBuffers &common_buffers, FgResRef depth_tex,
                                              FgResRef &inout_velocity_tex) {
    using Stg = Ren::eStageBits;

    auto &static_vel = fg_builder_.AddNode("FILL STATIC VEL");

    struct PassData {
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef velocity_tex;
    };

    auto *data = static_vel.AllocNodeData<PassData>();

    data->shared_data =
        static_vel.AddUniformBufferInput(common_buffers.shared_data, Stg::VertexShader | Stg::FragmentShader);
    data->depth_tex = static_vel.AddCustomTextureInput(depth_tex, Ren::eResState::StencilTestDepthFetch,
                                                       Stg::DepthAttachment | Stg::FragmentShader);
    inout_velocity_tex = data->velocity_tex = static_vel.AddColorOutput(inout_velocity_tex);

    static_vel.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocBuf &unif_shared_data_buf = builder.GetReadBuffer(data->shared_data);

        FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        FgAllocTex &velocity_tex = builder.GetWriteTexture(data->velocity_tex);

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.stencil.enabled = true;
        rast_state.stencil.write_mask = 0x00;
        rast_state.stencil.compare_op = unsigned(Ren::eCompareOp::Equal);

        rast_state.viewport[2] = view_state_.act_res[0];
        rast_state.viewport[3] = view_state_.act_res[1];

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::Tex2DSampled, BlitStaticVel::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(SharedDataBlock), *unif_shared_data_buf.ref}};

        const Ren::RenderTarget render_targets[] = {{velocity_tex.ref, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
        const Ren::RenderTarget depth_target = {depth_tex.ref, Ren::eLoadOp::None, Ren::eStoreOp::None,
                                                Ren::eLoadOp::Load, Ren::eStoreOp::None};

        BlitStaticVel::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]), float(view_state_.act_res[1])};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_static_vel_prog_, depth_target, render_targets, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(BlitStaticVel::Params), 0);
    });
}

void Eng::Renderer::AddTaaPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures,
                               const bool static_accumulation, FgResRef &resolved_color) {
    using Stg = Ren::eStageBits;

    auto &taa = fg_builder_.AddNode("TAA");

    struct PassData {
        FgResRef clean_tex;
        FgResRef depth_tex;
        FgResRef velocity_tex;
        FgResRef history_tex;

        FgResRef output_tex;
        FgResRef output_history_tex;
    };

    auto *data = taa.AllocNodeData<PassData>();
    data->clean_tex = taa.AddTextureInput(frame_textures.color, Stg::FragmentShader);
    data->depth_tex = taa.AddTextureInput(frame_textures.depth, Stg::FragmentShader);
    data->velocity_tex = taa.AddTextureInput(frame_textures.velocity, Stg::FragmentShader);

    { // Texture that holds resolved color
        Ren::Tex2DParams params;
        params.w = view_state_.scr_res[0];
        params.h = view_state_.scr_res[1];
        params.format = Ren::eTexFormat::RGBA16F;
        params.sampling.filter = Ren::eTexFilter::Bilinear;
        params.sampling.wrap = Ren::eTexWrap::ClampToBorder;

        resolved_color = data->output_tex = taa.AddColorOutput("Resolved Color", params);
        data->output_history_tex = taa.AddColorOutput("Color History", params);
    }
    data->history_tex = taa.AddHistoryTextureInput(data->output_history_tex, Stg::FragmentShader);

    taa.set_execute_cb([this, data, static_accumulation](FgBuilder &builder) {
        FgAllocTex &clean_tex = builder.GetReadTexture(data->clean_tex);
        FgAllocTex &depth_tex = builder.GetReadTexture(data->depth_tex);
        FgAllocTex &velocity_tex = builder.GetReadTexture(data->velocity_tex);
        FgAllocTex &history_tex = builder.GetReadTexture(data->history_tex);
        FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);
        FgAllocTex &output_history_tex = builder.GetWriteTexture(data->output_history_tex);

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

        rast_state.viewport[2] = view_state_.act_res[0];
        rast_state.viewport[3] = view_state_.act_res[1];

        { // Blit taa
            const Ren::RenderTarget render_targets[] = {
                {output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store},
                {output_history_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, TempAA::CURR_NEAREST_TEX_SLOT, {*clean_tex.ref, *nearest_sampler_}},
                {Ren::eBindTarget::Tex2DSampled, TempAA::CURR_LINEAR_TEX_SLOT, *clean_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, TempAA::HIST_TEX_SLOT, *history_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, TempAA::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Ren::eBindTarget::Tex2DSampled, TempAA::VELOCITY_TEX_SLOT, *velocity_tex.ref}};

            TempAA::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_.act_res[0], view_state_.act_res[1]};
            uniform_params.tex_size = Ren::Vec2f{float(view_state_.act_res[0]), float(view_state_.act_res[1])};
            uniform_params.significant_change =
                Dot(p_list_->env.sun_dir, view_state_.prev_sun_dir) < 0.99999f ? 1.0f : 0.0f;
            uniform_params.frame_index = float(view_state_.frame_index % 256);
            if (static_accumulation && int(accumulated_frames_) < RendererInternal::TaaSampleCountStatic) {
                uniform_params.mix_factor = 1.0f / (1.0f + accumulated_frames_);
            } else {
                uniform_params.mix_factor = 0.0f;
            }
            ++accumulated_frames_;

            prim_draw_.DrawPrim(
                PrimDraw::ePrim::Quad,
                static_accumulation ? blit_taa_static_prog_ : blit_taa_prog_[settings.enable_motion_blur], {},
                render_targets, rast_state, builder.rast_state(), bindings, &uniform_params, sizeof(TempAA::Params), 0);
        }
    });
}

void Eng::Renderer::AddDownsampleDepthPass(const CommonBuffers &common_buffers, FgResRef depth_tex,
                                           FgResRef &out_depth_down_2x) {
    auto &downsample_depth = fg_builder_.AddNode("DOWN DEPTH");

    struct PassData {
        FgResRef in_depth_tex;
        FgResRef out_depth_tex;
    };

    auto *data = downsample_depth.AllocNodeData<PassData>();
    data->in_depth_tex = downsample_depth.AddTextureInput(depth_tex, Ren::eStageBits::FragmentShader);

    { // Texture that holds 2x downsampled linear depth
        Ren::Tex2DParams params;
        params.w = view_state_.scr_res[0] / 2;
        params.h = view_state_.scr_res[1] / 2;
        params.format = Ren::eTexFormat::R32F;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        out_depth_down_2x = data->out_depth_tex = downsample_depth.AddColorOutput(DEPTH_DOWN_2X_TEX, params);
    }

    downsample_depth.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocTex &depth_tex = builder.GetReadTexture(data->in_depth_tex);
        FgAllocTex &output_tex = builder.GetWriteTexture(data->out_depth_tex);

        Ren::RastState rast_state;

        rast_state.viewport[2] = view_state_.act_res[0] / 2;
        rast_state.viewport[3] = view_state_.act_res[1] / 2;

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::Tex2DSampled, DownDepth::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}}};

        DownDepth::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]), float(view_state_.act_res[1])};
        uniform_params.clip_info = view_state_.clip_info;
        uniform_params.linearize = 1.0f;

        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down_depth_prog_, {}, render_targets, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(DownDepth::Params), 0);
    });
}

Eng::FgResRef Eng::Renderer::AddBloomPasses(FgResRef hdr_texture, FgResRef exposure_texture, const bool compressed) {
    static const int BloomMipCount = 5;

    Eng::FgResRef downsampled[BloomMipCount];
    for (int mip = 0; mip < BloomMipCount; ++mip) {
        const std::string node_name = "BLOOM DOWNS. " + std::to_string(mip) + "->" + std::to_string(mip + 1);
        auto &bloom_downsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgResRef input_tex;
            FgResRef exposure_tex;
            FgResRef output_tex;
        };

        auto *data = bloom_downsample.AllocNodeData<PassData>();
        if (mip == 0) {
            data->input_tex = bloom_downsample.AddTextureInput(hdr_texture, Ren::eStageBits::ComputeShader);
        } else {
            data->input_tex = bloom_downsample.AddTextureInput(downsampled[mip - 1], Ren::eStageBits::ComputeShader);
        }
        data->exposure_tex = bloom_downsample.AddTextureInput(exposure_texture, Ren::eStageBits::ComputeShader);

        { // Texture that holds downsampled bloom image
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] / 2) >> mip;
            params.h = (view_state_.scr_res[1] / 2) >> mip;
            params.format = compressed ? Ren::eTexFormat::RGBA16F : Ren::eTexFormat::RGBA32F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            const std::string output_name = "Bloom Downsampled " + std::to_string(mip);
            downsampled[mip] = data->output_tex =
                bloom_downsample.AddStorageImageOutput(output_name, params, Ren::eStageBits::ComputeShader);
        }

        bloom_downsample.set_execute_cb([this, data, mip](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &exposure_tex = builder.GetReadTexture(data->exposure_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = output_tex.ref->params.w;
            uniform_params.img_size[1] = output_tex.ref->params.h;
            uniform_params.pre_exposure = view_state_.pre_exposure;

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, Bloom::INPUT_TEX_SLOT, {*input_tex.ref, *linear_sampler_}},
                {Ren::eBindTarget::Tex2DSampled, Bloom::EXPOSURE_TEX_SLOT, *exposure_tex.ref},
                {Ren::eBindTarget::Image2D, Bloom::OUT_IMG_SLOT, *output_tex.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (uniform_params.img_size[0] + Bloom::LOCAL_GROUP_SIZE_X - 1u) / Bloom::LOCAL_GROUP_SIZE_X,
                (uniform_params.img_size[1] + Bloom::LOCAL_GROUP_SIZE_Y - 1u) / Bloom::LOCAL_GROUP_SIZE_Y, 1u};

            DispatchCompute(*pi_bloom_downsample_[mip == 0], grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    Eng::FgResRef upsampled[BloomMipCount - 1];
    for (int mip = BloomMipCount - 2; mip >= 0; --mip) {
        const std::string node_name = "BLOOM UPS. " + std::to_string(mip + 2) + "->" + std::to_string(mip + 1);
        auto &bloom_upsample = fg_builder_.AddNode(node_name);

        struct PassData {
            FgResRef input_tex;
            FgResRef blend_tex;
            FgResRef output_tex;
        };

        auto *data = bloom_upsample.AllocNodeData<PassData>();
        if (mip == BloomMipCount - 2) {
            data->input_tex = bloom_upsample.AddTextureInput(downsampled[mip + 1], Ren::eStageBits::ComputeShader);
        } else {
            data->input_tex = bloom_upsample.AddTextureInput(upsampled[mip + 1], Ren::eStageBits::ComputeShader);
        }
        data->blend_tex = bloom_upsample.AddTextureInput(downsampled[mip], Ren::eStageBits::ComputeShader);

        { // Texture that holds upsampled bloom image
            Ren::Tex2DParams params;
            params.w = (view_state_.scr_res[0] / 2) >> mip;
            params.h = (view_state_.scr_res[1] / 2) >> mip;
            params.format = compressed ? Ren::eTexFormat::RGBA16F : Ren::eTexFormat::RGBA32F;
            params.sampling.filter = Ren::eTexFilter::Bilinear;
            params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

            const std::string output_name = "Bloom Upsampled " + std::to_string(mip);
            upsampled[mip] = data->output_tex =
                bloom_upsample.AddStorageImageOutput(output_name, params, Ren::eStageBits::ComputeShader);
        }

        bloom_upsample.set_execute_cb([this, data, mip](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(data->input_tex);
            FgAllocTex &blend_tex = builder.GetReadTexture(data->blend_tex);
            FgAllocTex &output_tex = builder.GetWriteTexture(data->output_tex);

            Bloom::Params uniform_params;
            uniform_params.img_size[0] = output_tex.ref->params.w;
            uniform_params.img_size[1] = output_tex.ref->params.h;
            uniform_params.blend_weight = 1.0f / float(1 + BloomMipCount - 1 - mip);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::Tex2DSampled, Bloom::INPUT_TEX_SLOT, *input_tex.ref},
                                             {Ren::eBindTarget::Tex2DSampled, Bloom::BLEND_TEX_SLOT, *blend_tex.ref},
                                             {Ren::eBindTarget::Image2D, Bloom::OUT_IMG_SLOT, *output_tex.ref}};

            const Ren::Vec3u grp_count = Ren::Vec3u{
                (uniform_params.img_size[0] + Bloom::LOCAL_GROUP_SIZE_X - 1u) / Bloom::LOCAL_GROUP_SIZE_X,
                (uniform_params.img_size[1] + Bloom::LOCAL_GROUP_SIZE_Y - 1u) / Bloom::LOCAL_GROUP_SIZE_Y, 1u};

            DispatchCompute(*pi_bloom_upsample_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.log());
        });
    }

    return upsampled[0];
}

Eng::FgResRef Eng::Renderer::AddAutoexposurePasses(FgResRef hdr_texture) {
    FgResRef histogram;
    { // Clear histogram image
        auto &histogram_clear = fg_builder_.AddNode("HISTOGRAM CLEAR");

        Ren::Tex2DParams params;
        params.w = EXPOSURE_HISTOGRAM_RES + 1;
        params.h = 1;
        params.format = Ren::eTexFormat::R32UI;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        histogram = histogram_clear.AddTransferImageOutput("Exposure Histogram", params);

        histogram_clear.set_execute_cb([histogram](FgBuilder &builder) {
            FgAllocTex &histogram_tex = builder.GetWriteTexture(histogram);

            const float zero[4] = {};
            Ren::ClearImage(*histogram_tex.ref, zero, builder.ctx().current_cmd_buf());
        });
    }
    { // Sample histogram
        auto &histogram_sample = fg_builder_.AddNode("HISTOGRAM SAMPLE");

        FgResRef input = histogram_sample.AddTextureInput(hdr_texture, Ren::eStageBits::ComputeShader);
        histogram = histogram_sample.AddStorageImageOutput(histogram, Ren::eStageBits::ComputeShader);

        histogram_sample.set_execute_cb([this, input, histogram](FgBuilder &builder) {
            FgAllocTex &input_tex = builder.GetReadTexture(input);
            FgAllocTex &output_tex = builder.GetWriteTexture(histogram);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, HistogramSample::HDR_TEX_SLOT, {*input_tex.ref, *linear_sampler_}},
                {Ren::eBindTarget::Image2D, HistogramSample::OUT_IMG_SLOT, *output_tex.ref}};

            HistogramSample::Params uniform_params = {};
            uniform_params.pre_exposure = view_state_.pre_exposure;

            DispatchCompute(*pi_histogram_sample_, Ren::Vec3u{16, 8, 1}, bindings, &uniform_params,
                            sizeof(uniform_params), builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    FgResRef exposure;
    { // Calc exposure
        auto &histogram_exposure = fg_builder_.AddNode("HISTOGRAM EXPOSURE");

        struct PassData {
            FgResRef histogram;
            FgResRef exposure_prev;
            FgResRef exposure;
        };

        auto *data = histogram_exposure.AllocNodeData<PassData>();

        data->histogram = histogram_exposure.AddTextureInput(histogram, Ren::eStageBits::ComputeShader);

        Ren::Tex2DParams params;
        params.w = params.h = 1;
        params.format = Ren::eTexFormat::R32F;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;
        exposure = data->exposure =
            histogram_exposure.AddStorageImageOutput(EXPOSURE_TEX, params, Ren::eStageBits::ComputeShader);
        data->exposure_prev = histogram_exposure.AddHistoryTextureInput(exposure, Ren::eStageBits::ComputeShader);

        histogram_exposure.set_execute_cb([this, data](FgBuilder &builder) {
            FgAllocTex &histogram_tex = builder.GetReadTexture(data->histogram);
            FgAllocTex &exposure_prev_tex = builder.GetReadTexture(data->exposure_prev);
            FgAllocTex &exposure_tex = builder.GetWriteTexture(data->exposure);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::Tex2DSampled, HistogramExposure::HISTOGRAM_TEX_SLOT, *histogram_tex.ref},
                {Ren::eBindTarget::Tex2DSampled, HistogramExposure::EXPOSURE_PREV_TEX_SLOT, *exposure_prev_tex.ref},
                {Ren::eBindTarget::Image2D, HistogramExposure::OUT_TEX_SLOT, *exposure_tex.ref}};

            HistogramExposure::Params uniform_params = {};
            uniform_params.min_exposure = min_exposure_;
            uniform_params.max_exposure = max_exposure_;
            uniform_params.exposure_factor = (settings.tonemap_mode != Eng::eTonemapMode::Standard) ? 1.25f : 0.5f;

            DispatchCompute(*pi_histogram_exposure_, Ren::Vec3u{1}, bindings, &uniform_params, sizeof(uniform_params),
                            builder.ctx().default_descr_alloc(), builder.log());
        });
    }
    return exposure;
}

void Eng::Renderer::AddDebugVelocityPass(const FgResRef velocity, FgResRef &output_tex) {
    auto &debug_motion = fg_builder_.AddNode("DEBUG MOTION");

    struct PassData {
        FgResRef in_velocity_tex;
        FgResRef out_color_tex;
    };

    auto *data = debug_motion.AllocNodeData<PassData>();
    data->in_velocity_tex = debug_motion.AddTextureInput(velocity, Ren::eStageBits::ComputeShader);

    { // Output texture
        Ren::Tex2DParams params;
        params.w = view_state_.scr_res[0];
        params.h = view_state_.scr_res[1];
        params.format = Ren::eTexFormat::RGBA8;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex = data->out_color_tex =
            debug_motion.AddStorageImageOutput("Velocity Debug", params, Ren::eStageBits::ComputeShader);
    }

    debug_motion.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocTex &velocity_tex = builder.GetReadTexture(data->in_velocity_tex);
        FgAllocTex &output_tex = builder.GetWriteTexture(data->out_color_tex);

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::Tex2DSampled, DebugVelocity::VELOCITY_TEX_SLOT, *velocity_tex.ref},
            {Ren::eBindTarget::Image2D, DebugVelocity::OUT_IMG_SLOT, *output_tex.ref}};

        const Ren::Vec3u grp_count = Ren::Vec3u{
            (view_state_.act_res[0] + DebugVelocity::LOCAL_GROUP_SIZE_X - 1u) / DebugVelocity::LOCAL_GROUP_SIZE_X,
            (view_state_.act_res[1] + DebugVelocity::LOCAL_GROUP_SIZE_Y - 1u) / DebugVelocity::LOCAL_GROUP_SIZE_Y, 1u};

        DebugVelocity::Params uniform_params;
        uniform_params.img_size[0] = view_state_.act_res[0];
        uniform_params.img_size[1] = view_state_.act_res[1];

        DispatchCompute(*pi_debug_velocity_, grp_count, bindings, &uniform_params, sizeof(uniform_params),
                        ctx_.default_descr_alloc(), ctx_.log());
    });
}
