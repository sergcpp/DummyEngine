#include "Renderer.h"

#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/ScopeExit.h>
#include <Sys/Time_.h>

#include "../utils/Random.h"
#include "../utils/ShaderLoader.h"
#include "Renderer_Names.h"
#include "executors/ExEmissive.h"
#include "executors/ExGBufferFill.h"
#include "executors/ExOpaque.h"
#include "executors/ExTransparent.h"

#include "shaders/blit_down_depth_interface.h"
#include "shaders/blit_down_interface.h"
#include "shaders/blit_gauss_interface.h"
#include "shaders/blit_static_vel_interface.h"
#include "shaders/blit_taa_interface.h"
#include "shaders/debug_velocity_interface.h"
#include "shaders/gbuffer_shade_interface.h"

namespace RendererInternal {
const float GoldenRatio = 1.61803398875f;
extern const int TaaSampleCountStatic;
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
    pi_probe_classify_[2] = sh_.LoadPipeline("internal/probe_classify@VOL.comp.glsl");
    pi_probe_classify_[3] = sh_.LoadPipeline("internal/probe_classify@VOL;PARTIAL.comp.glsl");
    pi_probe_classify_[4] = sh_.LoadPipeline("internal/probe_classify@RESET.comp.glsl");
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
    pi_sun_shadows_[0] = sh_.LoadPipeline("internal/sun_shadows@SS_SHADOW.comp.glsl");
    pi_sun_shadows_[1] = sh_.LoadPipeline("internal/sun_shadows@RT_SHADOW.comp.glsl");
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
    pi_bloom_downsample_[0][0] = sh_.LoadPipeline("internal/bloom_downsample.comp.glsl");
    pi_bloom_downsample_[0][1] = sh_.LoadPipeline("internal/bloom_downsample@TONEMAP.comp.glsl");
    pi_bloom_downsample_[1][0] = sh_.LoadPipeline("internal/bloom_downsample@COMPRESSED.comp.glsl");
    pi_bloom_downsample_[1][1] = sh_.LoadPipeline("internal/bloom_downsample@COMPRESSED;TONEMAP.comp.glsl");
    pi_bloom_upsample_ = sh_.LoadPipeline("internal/bloom_upsample.comp.glsl");

    // Autoexposure
    pi_histogram_sample_ = sh_.LoadPipeline("internal/histogram_sample.comp.glsl");
    pi_histogram_exposure_ = sh_.LoadPipeline("internal/histogram_exposure.comp.glsl");

    // Volumetrics
    pi_sky_upsample_ = sh_.LoadPipeline("internal/skydome_upsample.comp.glsl");
    pi_vol_scatter_[0][0] = sh_.LoadPipeline(subgroup_select("internal/vol_scatter.comp.glsl", //
                                                             "internal/vol_scatter@NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[0][1] = sh_.LoadPipeline(subgroup_select("internal/vol_scatter@GI_CACHE.comp.glsl", //
                                                             "internal/vol_scatter@GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[1][0] =
        sh_.LoadPipeline(subgroup_select("internal/vol_scatter@ALL_CASCADES.comp.glsl", //
                                         "internal/vol_scatter@ALL_CASCADES;NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[1][1] =
        sh_.LoadPipeline(subgroup_select("internal/vol_scatter@ALL_CASCADES;GI_CACHE.comp.glsl", //
                                         "internal/vol_scatter@ALL_CASCADES;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_vol_ray_march_ = sh_.LoadPipeline("internal/vol_ray_march.comp.glsl");

    // Debugging
    pi_debug_velocity_ = sh_.LoadPipeline("internal/debug_velocity.comp.glsl");

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    blit_static_vel_prog_ = sh_.LoadProgram("internal/blit_static_vel.vert.glsl", "internal/blit_static_vel.frag.glsl");
    blit_gauss_prog_ = sh_.LoadProgram("internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
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
    blit_down_prog_ = sh_.LoadProgram("internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
    blit_down_depth_prog_ = sh_.LoadProgram("internal/blit_down_depth.vert.glsl", "internal/blit_down_depth.frag.glsl");
    blit_fxaa_prog_ = sh_.LoadProgram("internal/blit_fxaa.vert.glsl", "internal/blit_fxaa.frag.glsl");
    blit_vol_compose_prog_ =
        sh_.LoadProgram("internal/blit_vol_compose.vert.glsl", "internal/blit_vol_compose.frag.glsl");
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
        desc.views.push_back(Ren::eTexFormat::RG32UI);
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
            *skin_transforms_buf.ref, 0, uint32_t(p_list_->skin_transforms.size() * sizeof(skin_transform_t)),
            p_list_->skin_transforms.data(), *p_list_->skin_transforms_stage_buf,
            ctx.backend_frame() * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize, ctx.current_cmd_buf());

        Ren::UpdateBuffer(*shape_keys_buf.ref, 0, p_list_->shape_keys_data.count * sizeof(shape_key_data_t),
                          p_list_->shape_keys_data.data, *p_list_->shape_keys_stage_buf,
                          ctx.backend_frame() * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize, ctx.current_cmd_buf());

        Ren::UpdateBuffer(*instance_indices_buf.ref, 0, uint32_t(p_list_->instance_indices.size() * sizeof(Ren::Vec2i)),
                          p_list_->instance_indices.data(), *p_list_->instance_indices_stage_buf,
                          ctx.backend_frame() * InstanceIndicesBufChunkSize, InstanceIndicesBufChunkSize,
                          ctx.current_cmd_buf());

        { // Prepare data that is shared for all instances
            shared_data_t shrd_data;

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

            for (int i = 0; i < 6; ++i) {
                shrd_data.frustum_planes[i] =
                    Ren::Vec4f(p_list_->draw_cam.frustum_plane(i).n, p_list_->draw_cam.frustum_plane(i).d);
            }

            shrd_data.clip_from_view[2][0] += p_list_->draw_cam.px_offset()[0];
            shrd_data.clip_from_view[2][1] += p_list_->draw_cam.px_offset()[1];

            shrd_data.clip_from_world = view_state_.clip_from_world =
                (shrd_data.clip_from_view * shrd_data.view_from_world);

            Ren::Mat4f view_from_world_no_translation = shrd_data.view_from_world;
            view_from_world_no_translation[3][0] = view_from_world_no_translation[3][1] =
                view_from_world_no_translation[3][2] = 0;
            view_state_.clip_from_world_no_translation = (shrd_data.clip_from_view * view_from_world_no_translation);

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
                       sizeof(shadow_map_region_t) * p_list_->shadow_regions.count);
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

            memcpy(&shrd_data.probes[0], p_list_->probes.data(), sizeof(probe_item_t) * p_list_->probes.size());
            memcpy(&shrd_data.ellipsoids[0], p_list_->ellipsoids.data(),
                   sizeof(ellipse_item_t) * p_list_->ellipsoids.size());

            const int portals_count = std::min(int(p_list_->portals.size()), MAX_PORTALS_TOTAL);
            memcpy(&shrd_data.portals[0], p_list_->portals.data(), portals_count * sizeof(uint32_t));
            if (portals_count < MAX_PORTALS_TOTAL) {
                shrd_data.portals[portals_count] = 0xffffffff;
            }

            memcpy(&shrd_data.atmosphere, &p_list_->env.atmosphere, sizeof(atmosphere_params_t));
            static_assert(sizeof(Eng::atmosphere_params_t) == sizeof(Types::atmosphere_params_t));

            Ren::UpdateBuffer(*shared_data_buf.ref, 0, sizeof(shared_data_t), &shrd_data,
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
        desc.views.push_back(Ren::eTexFormat::RG32UI);
        common_buffers.cells = update_light_bufs.AddTransferOutput("Cells Buffer", desc);
    }
    { // create RT cells buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        desc.views.push_back(Ren::eTexFormat::RG32UI);
        common_buffers.rt_cells = update_light_bufs.AddTransferOutput("RT Cells Buffer", desc);
    }
    { // create lights buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        desc.views.push_back(Ren::eTexFormat::RGBA32F);
        common_buffers.lights = update_light_bufs.AddTransferOutput("Lights Buffer", desc);
    }
    { // create decals buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        desc.views.push_back(Ren::eTexFormat::RGBA32F);
        common_buffers.decals = update_light_bufs.AddTransferOutput("Decals Buffer", desc);
    }
    { // create items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        desc.views.push_back(Ren::eTexFormat::RG32UI);
        common_buffers.items = update_light_bufs.AddTransferOutput("Items Buffer", desc);
    }
    { // create RT items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        desc.views.push_back(Ren::eTexFormat::RG32UI);
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

        Ren::UpdateBuffer(*cells_buf.ref, 0, p_list_->cells.count * sizeof(cell_data_t), p_list_->cells.data,
                          *p_list_->cells_stage_buf, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize,
                          ctx.current_cmd_buf());

        Ren::UpdateBuffer(*rt_cells_buf.ref, 0, p_list_->rt_cells.count * sizeof(cell_data_t), p_list_->rt_cells.data,
                          *p_list_->rt_cells_stage_buf, ctx.backend_frame() * CellsBufChunkSize, CellsBufChunkSize,
                          ctx.current_cmd_buf());

        Ren::UpdateBuffer(*lights_buf.ref, 0, uint32_t(p_list_->lights.size() * sizeof(light_item_t)),
                          p_list_->lights.data(), *p_list_->lights_stage_buf, ctx.backend_frame() * LightsBufChunkSize,
                          LightsBufChunkSize, ctx.current_cmd_buf());

        Ren::UpdateBuffer(*decals_buf.ref, 0, uint32_t(p_list_->decals.size() * sizeof(decal_item_t)),
                          p_list_->decals.data(), *p_list_->decals_stage_buf, ctx.backend_frame() * DecalsBufChunkSize,
                          DecalsBufChunkSize, ctx.current_cmd_buf());

        if (p_list_->items.count) {
            Ren::UpdateBuffer(*items_buf.ref, 0, p_list_->items.count * sizeof(item_data_t), p_list_->items.data,
                              *p_list_->items_stage_buf, ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize,
                              ctx.current_cmd_buf());
        } else {
            const item_data_t dummy = {};
            Ren::UpdateBuffer(*items_buf.ref, 0, sizeof(item_data_t), &dummy, *p_list_->items_stage_buf,
                              ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
        }

        if (p_list_->rt_items.count) {
            Ren::UpdateBuffer(*rt_items_buf.ref, 0, p_list_->rt_items.count * sizeof(item_data_t),
                              p_list_->rt_items.data, *p_list_->rt_items_stage_buf,
                              ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
        } else {
            const item_data_t dummy = {};
            Ren::UpdateBuffer(*rt_items_buf.ref, 0, sizeof(item_data_t), &dummy, *p_list_->rt_items_stage_buf,
                              ctx.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, ctx.current_cmd_buf());
        }
    });
}

void Eng::Renderer::AddGBufferFillPass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                       const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

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

    const FgResRef noise_tex =
        gbuf_fill.AddTextureInput(noise_tex_, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgResRef dummy_white = gbuf_fill.AddTextureInput(dummy_white_, Stg::FragmentShader);
    const FgResRef dummy_black = gbuf_fill.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = gbuf_fill.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shared_data_buf = gbuf_fill.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    const FgResRef cells_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgResRef items_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgResRef decals_buf = gbuf_fill.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    frame_textures.albedo = gbuf_fill.AddColorOutput(MAIN_ALBEDO_TEX, frame_textures.albedo_params);
    frame_textures.normal = gbuf_fill.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_params);
    frame_textures.specular = gbuf_fill.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_params);
    frame_textures.depth = gbuf_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    gbuf_fill.make_executor<ExGBufferFill>(
        &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf, &bindless, noise_tex,
        dummy_white, dummy_black, instances_buf, instances_indices_buf, shared_data_buf, cells_buf, items_buf,
        decals_buf, frame_textures.albedo, frame_textures.normal, frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                         const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

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
    const FgResRef noise_tex =
        opaque.AddTextureInput(noise_tex_, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgResRef cone_rt_lut = opaque.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

    const FgResRef dummy_black = opaque.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = opaque.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        opaque.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shader_data_buf =
        opaque.AddUniformBufferInput(common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

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

    opaque.make_executor<ExOpaque>(
        &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
        persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black, instances_buf,
        instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf, decals_buf, shadowmap_tex, ssao_tex,
        lmap_tex, frame_textures.color, frame_textures.normal, frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddForwardTransparentPass(const CommonBuffers &common_buffers,
                                              const PersistentGpuData &persistent_data,
                                              const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

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
    const FgResRef noise_tex =
        transparent.AddTextureInput(noise_tex_, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgResRef cone_rt_lut = transparent.AddTextureInput(cone_rt_lut_, Stg::FragmentShader);

    const FgResRef dummy_black = transparent.AddTextureInput(dummy_black_, Stg::FragmentShader);

    const FgResRef instances_buf = transparent.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        transparent.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shader_data_buf = transparent.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

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

    transparent.make_executor<ExTransparent>(
        &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
        persistent_data.pipelines.data(), &bindless, brdf_lut, noise_tex, cone_rt_lut, dummy_black, instances_buf,
        instances_indices_buf, shader_data_buf, cells_buf, items_buf, lights_buf, decals_buf, shadowmap_tex, ssao_tex,
        lmap_tex, frame_textures.color, frame_textures.normal, frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures,
                                           const bool enable_gi) {
    using Stg = Ren::eStage;
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
            {Trg::UTBuf, GBufferShade::CELLS_BUF_SLOT, *cells_buf.ref},
            {Trg::UTBuf, GBufferShade::ITEMS_BUF_SLOT, *items_buf.ref},
            {Trg::UTBuf, GBufferShade::LIGHT_BUF_SLOT, *lights_buf.ref},
            {Trg::UTBuf, GBufferShade::DECAL_BUF_SLOT, *decals_buf.ref},
            {Trg::TexSampled, GBufferShade::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Trg::TexSampled, GBufferShade::DEPTH_LIN_TEX_SLOT, {*depth_tex.ref, *linear_sampler_, 1}},
            {Trg::TexSampled, GBufferShade::ALBEDO_TEX_SLOT, *albedo_tex.ref},
            {Trg::TexSampled, GBufferShade::NORM_TEX_SLOT, *normal_tex.ref},
            {Trg::TexSampled, GBufferShade::SPEC_TEX_SLOT, *spec_tex.ref},
            {Trg::TexSampled, GBufferShade::SHADOW_DEPTH_TEX_SLOT, *shad_depth_tex.ref},
            {Trg::TexSampled, GBufferShade::SHADOW_DEPTH_VAL_TEX_SLOT, {*shad_depth_tex.ref, *nearest_sampler_}},
            {Trg::TexSampled, GBufferShade::SHADOW_COLOR_TEX_SLOT, *shad_color_tex.ref},
            {Trg::TexSampled, GBufferShade::SSAO_TEX_SLOT, *ssao_tex.ref},
            {Trg::TexSampled, GBufferShade::GI_TEX_SLOT, *gi_tex.ref},
            {Trg::TexSampled, GBufferShade::SUN_SHADOW_TEX_SLOT, *sun_shadow_tex.ref},
            {Trg::TexSampled, GBufferShade::LTC_LUTS_TEX_SLOT, *ltc_luts.ref},
            {Trg::TexSampled, GBufferShade::ENV_TEX_SLOT, *env_tex.ref},
            {Trg::ImageRW, GBufferShade::OUT_COLOR_IMG_SLOT, *out_color_tex.ref}};

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
    using Stg = Ren::eStage;

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

    const FgResRef noise_tex =
        emissive.AddTextureInput(noise_tex_, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgResRef dummy_white = emissive.AddTextureInput(dummy_white_, Stg::FragmentShader);

    const FgResRef instances_buf = emissive.AddStorageReadonlyInput(persistent_data.instance_buf, Stg::VertexShader);
    const FgResRef instances_indices_buf =
        emissive.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgResRef shared_data_buf = emissive.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    frame_textures.color = emissive.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_params);
    frame_textures.depth = emissive.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_params);

    emissive.make_executor<ExEmissive>(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials_buf, textures_buf,
                                       &bindless, noise_tex, dummy_white, instances_buf, instances_indices_buf,
                                       shared_data_buf, frame_textures.color, frame_textures.depth);
}

void Eng::Renderer::AddFillStaticVelocityPass(const CommonBuffers &common_buffers, FgResRef depth_tex,
                                              FgResRef &inout_velocity_tex) {
    using Stg = Ren::eStage;

    auto &static_vel = fg_builder_.AddNode("FILL STATIC VEL");

    struct PassData {
        FgResRef shared_data;
        FgResRef depth_tex;
        FgResRef velocity_tex;
    };

    auto *data = static_vel.AllocNodeData<PassData>();

    data->shared_data = static_vel.AddUniformBufferInput(common_buffers.shared_data,
                                                         Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    data->depth_tex = static_vel.AddCustomTextureInput(depth_tex, Ren::eResState::StencilTestDepthFetch,
                                                       Ren::Bitmask{Stg::DepthAttachment} | Stg::FragmentShader);
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
            {Ren::eBindTarget::TexSampled, BlitStaticVel::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
            {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), *unif_shared_data_buf.ref}};

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
    using Stg = Ren::eStage;

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
        Ren::TexParams params;
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
                {Ren::eBindTarget::TexSampled, TempAA::CURR_NEAREST_TEX_SLOT, {*clean_tex.ref, *nearest_sampler_}},
                {Ren::eBindTarget::TexSampled, TempAA::CURR_LINEAR_TEX_SLOT, *clean_tex.ref},
                {Ren::eBindTarget::TexSampled, TempAA::HIST_TEX_SLOT, *history_tex.ref},
                {Ren::eBindTarget::TexSampled, TempAA::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                {Ren::eBindTarget::TexSampled, TempAA::VELOCITY_TEX_SLOT, *velocity_tex.ref}};

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
    data->in_depth_tex = downsample_depth.AddTextureInput(depth_tex, Ren::eStage::FragmentShader);

    { // Texture that holds 2x downsampled linear depth
        Ren::TexParams params;
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
            {Ren::eBindTarget::TexSampled, DownDepth::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}}};

        DownDepth::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_.act_res[0]), float(view_state_.act_res[1])};
        uniform_params.clip_info = view_state_.clip_info;
        uniform_params.linearize = 1.0f;

        const Ren::RenderTarget render_targets[] = {{output_tex.ref, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        prim_draw_.DrawPrim(PrimDraw::ePrim::Quad, blit_down_depth_prog_, {}, render_targets, rast_state,
                            builder.rast_state(), bindings, &uniform_params, sizeof(DownDepth::Params), 0);
    });
}

void Eng::Renderer::AddDebugVelocityPass(const FgResRef velocity, FgResRef &output_tex) {
    auto &debug_motion = fg_builder_.AddNode("DEBUG MOTION");

    struct PassData {
        FgResRef in_velocity_tex;
        FgResRef out_color_tex;
    };

    auto *data = debug_motion.AllocNodeData<PassData>();
    data->in_velocity_tex = debug_motion.AddTextureInput(velocity, Ren::eStage::ComputeShader);

    { // Output texture
        Ren::TexParams params;
        params.w = view_state_.scr_res[0];
        params.h = view_state_.scr_res[1];
        params.format = Ren::eTexFormat::RGBA8;
        params.sampling.wrap = Ren::eTexWrap::ClampToEdge;

        output_tex = data->out_color_tex =
            debug_motion.AddStorageImageOutput("Velocity Debug", params, Ren::eStage::ComputeShader);
    }

    debug_motion.set_execute_cb([this, data](FgBuilder &builder) {
        FgAllocTex &velocity_tex = builder.GetReadTexture(data->in_velocity_tex);
        FgAllocTex &output_tex = builder.GetWriteTexture(data->out_color_tex);

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::TexSampled, DebugVelocity::VELOCITY_TEX_SLOT, *velocity_tex.ref},
            {Ren::eBindTarget::ImageRW, DebugVelocity::OUT_IMG_SLOT, *output_tex.ref}};

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
