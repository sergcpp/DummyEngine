#include "Renderer.h"

#include <Ren/Context.h>
#include <Ren/utils/Utils.h>
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
#include "shaders/blit_tsr_interface.h"
#include "shaders/debug_gbuffer_interface.h"
#include "shaders/debug_velocity_interface.h"
#include "shaders/gbuffer_shade_interface.h"
#include "shaders/motion_blur_interface.h"
#include "shaders/prepare_disocclusion_interface.h"
#include "shaders/reconstruct_depth_interface.h"

namespace RendererInternal {
const float GoldenRatio = 1.61803398875f;
extern const int TaaSampleCountStatic;
} // namespace RendererInternal

void Eng::Renderer::InitPipelines() {
    auto subgroup_select = [this](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
        return ctx_.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
    };

    pi_gbuf_shade_[0] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/gbuffer_shade@SS_SHADOW_ONE.comp.glsl",
                                                 "internal/gbuffer_shade@SS_SHADOW_ONE;NO_SUBGROUP.comp.glsl"));
    pi_gbuf_shade_[1] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/gbuffer_shade@SHADOW_JITTER;SS_SHADOW_MANY.comp.glsl",
                        "internal/gbuffer_shade@SHADOW_JITTER;SS_SHADOW_MANY;NO_SUBGROUP.comp.glsl"));

    // Specular GI
    pi_specular_classify_ =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_specular_classify.comp.glsl", //
                                                 "internal/rt_specular_classify@NO_SUBGROUP.comp.glsl"));
    pi_specular_write_indirect_[0] =
        sh_.FindOrCreatePipeline("internal/rt_specular_write_indirect_args@MAIN.comp.glsl");
    pi_specular_write_indirect_[1] =
        sh_.FindOrCreatePipeline("internal/rt_specular_write_indirect_args@RT_DISPATCH.comp.glsl");
    pi_specular_write_indirect_[2] =
        sh_.FindOrCreatePipeline("internal/rt_specular_write_indirect_args@SHADE.comp.glsl");
    pi_specular_trace_ss_[0][0] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_specular_trace_ss.comp.glsl", //
                                                 "internal/rt_specular_trace_ss@NO_SUBGROUP.comp.glsl"));
    pi_specular_trace_ss_[0][1] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_specular_trace_ss@GI_CACHE.comp.glsl",
                                                 "internal/rt_specular_trace_ss@GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_specular_trace_ss_[1][0] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_specular_trace_ss@LAYERED.comp.glsl",
                                                 "internal/rt_specular_trace_ss@LAYERED;NO_SUBGROUP.comp.glsl"));
    pi_specular_trace_ss_[1][1] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_trace_ss@LAYERED;GI_CACHE.comp.glsl",
                        "internal/rt_specular_trace_ss@LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_specular_shade_[0] = sh_.FindOrCreatePipeline("internal/rt_specular_shade@MISS.comp.glsl");
    pi_specular_shade_[1] = sh_.FindOrCreatePipeline("internal/rt_specular_shade@MISS_SECOND.comp.glsl");
    pi_specular_shade_[2] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_specular_shade@HIT;GI_CACHE.comp.glsl",
                                                 "internal/rt_specular_shade@HIT;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_specular_shade_[3] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_shade@HIT;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                        "internal/rt_specular_shade@HIT;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"));
    pi_specular_shade_[4] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_shade@HIT_FIRST;GI_CACHE.comp.glsl",
                        "internal/rt_specular_shade@HIT_FIRST;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_specular_shade_[5] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_shade@HIT_FIRST;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                        "internal/rt_specular_shade@HIT_FIRST;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"));
    pi_specular_shade_[6] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_shade@HIT_SECOND;GI_CACHE.comp.glsl",
                        "internal/rt_specular_shade@HIT_SECOND;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    // OIT
    pi_specular_shade_[7] = sh_.FindOrCreatePipeline("internal/rt_specular_shade@MISS;LAYERED.comp.glsl");
    pi_specular_shade_[8] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_specular_shade@HIT;LAYERED;GI_CACHE.comp.glsl",
                        "internal/rt_specular_shade@HIT;LAYERED;GI_CACHE;NO_SUBGROUP.comp.glsl"));

    // Specular GI denoising
    pi_specular_reproject_ = sh_.FindOrCreatePipeline("internal/rt_specular_reproject.comp.glsl");
    pi_specular_temporal_[0] = sh_.FindOrCreatePipeline("internal/rt_specular_temporal.comp.glsl");
    pi_specular_temporal_[1] = sh_.FindOrCreatePipeline("internal/rt_specular_temporal@RELAXED.comp.glsl");
    pi_specular_filter_[0] = sh_.FindOrCreatePipeline("internal/rt_specular_filter@PRE_FILTER.comp.glsl");
    pi_specular_filter_[1] = sh_.FindOrCreatePipeline("internal/rt_specular_filter@PRE_FILTER;RELAXED.comp.glsl");
    pi_specular_filter_[2] = sh_.FindOrCreatePipeline("internal/rt_specular_filter.comp.glsl");
    pi_specular_filter_[3] = sh_.FindOrCreatePipeline("internal/rt_specular_filter@POST_FILTER.comp.glsl");
    pi_specular_stabilization_ = sh_.FindOrCreatePipeline("internal/rt_specular_stabilization.comp.glsl");
    pi_tile_clear_[0] = sh_.FindOrCreatePipeline("internal/tile_clear.comp.glsl");
    pi_tile_clear_[1] = sh_.FindOrCreatePipeline("internal/tile_clear@AVERAGE.comp.glsl");
    pi_tile_clear_[2] = sh_.FindOrCreatePipeline("internal/tile_clear@VARIANCE.comp.glsl");
    pi_tile_clear_[3] = sh_.FindOrCreatePipeline("internal/tile_clear@AVERAGE;VARIANCE.comp.glsl");

    // GI Cache
    pi_probe_blend_[0][0] = sh_.FindOrCreatePipeline("internal/probe_blend@RADIANCE.comp.glsl");
    pi_probe_blend_[1][0] = sh_.FindOrCreatePipeline("internal/probe_blend@RADIANCE;STOCH_LIGHTS.comp.glsl");
    pi_probe_blend_[2][0] = sh_.FindOrCreatePipeline("internal/probe_blend@DISTANCE.comp.glsl");
    pi_probe_blend_[0][1] = sh_.FindOrCreatePipeline("internal/probe_blend@RADIANCE;PARTIAL.comp.glsl");
    pi_probe_blend_[1][1] = sh_.FindOrCreatePipeline("internal/probe_blend@RADIANCE;STOCH_LIGHTS;PARTIAL.comp.glsl");
    pi_probe_blend_[2][1] = sh_.FindOrCreatePipeline("internal/probe_blend@DISTANCE;PARTIAL.comp.glsl");
    pi_probe_relocate_[0] = sh_.FindOrCreatePipeline("internal/probe_relocate.comp.glsl");
    pi_probe_relocate_[1] = sh_.FindOrCreatePipeline("internal/probe_relocate@PARTIAL.comp.glsl");
    pi_probe_relocate_[2] = sh_.FindOrCreatePipeline("internal/probe_relocate@RESET.comp.glsl");
    pi_probe_classify_[0] = sh_.FindOrCreatePipeline("internal/probe_classify.comp.glsl");
    pi_probe_classify_[1] = sh_.FindOrCreatePipeline("internal/probe_classify@PARTIAL.comp.glsl");
    pi_probe_classify_[2] = sh_.FindOrCreatePipeline("internal/probe_classify@VOL.comp.glsl");
    pi_probe_classify_[3] = sh_.FindOrCreatePipeline("internal/probe_classify@VOL;PARTIAL.comp.glsl");
    pi_probe_classify_[4] = sh_.FindOrCreatePipeline("internal/probe_classify@RESET.comp.glsl");
    pi_probe_sample_ = sh_.FindOrCreatePipeline("internal/probe_sample.comp.glsl");

    // GTAO
    pi_gtao_main_[0] = sh_.FindOrCreatePipeline("internal/gtao_main.comp.glsl");
    pi_gtao_main_[1] = sh_.FindOrCreatePipeline("internal/gtao_main@HALF_RES.comp.glsl");
    pi_gtao_filter_[0] = sh_.FindOrCreatePipeline("internal/gtao_filter.comp.glsl");
    pi_gtao_filter_[1] = sh_.FindOrCreatePipeline("internal/gtao_filter@HALF_RES.comp.glsl");
    pi_gtao_accumulate_[0] = sh_.FindOrCreatePipeline("internal/gtao_accumulate.comp.glsl");
    pi_gtao_accumulate_[1] = sh_.FindOrCreatePipeline("internal/gtao_accumulate@HALF_RES.comp.glsl");

    // GI
    pi_diffuse_classify_ =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_diffuse_classify.comp.glsl", //
                                                 "internal/rt_diffuse_classify@NO_SUBGROUP.comp.glsl"));
    pi_diffuse_write_indirect_[0] = sh_.FindOrCreatePipeline("internal/rt_diffuse_write_indirect_args@MAIN.comp.glsl");
    pi_diffuse_write_indirect_[1] =
        sh_.FindOrCreatePipeline("internal/rt_diffuse_write_indirect_args@RT_DISPATCH.comp.glsl");
    pi_diffuse_write_indirect_[2] = sh_.FindOrCreatePipeline("internal/rt_diffuse_write_indirect_args@SHADE.comp.glsl");
    pi_diffuse_trace_ss_ =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_diffuse_trace_ss.comp.glsl", //
                                                 "internal/rt_diffuse_trace_ss@NO_SUBGROUP.comp.glsl"));
    pi_diffuse_shade_[0] = sh_.FindOrCreatePipeline("internal/rt_diffuse_shade@MISS.comp.glsl");
    pi_diffuse_shade_[1] = sh_.FindOrCreatePipeline("internal/rt_diffuse_shade@MISS_SECOND.comp.glsl");
    pi_diffuse_shade_[2] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_diffuse_shade@HIT;GI_CACHE.comp.glsl",
                                                 "internal/rt_diffuse_shade@HIT;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_diffuse_shade_[3] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_diffuse_shade@HIT;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                        "internal/rt_diffuse_shade@HIT;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"));
    pi_diffuse_shade_[4] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_diffuse_shade@HIT_FIRST;GI_CACHE.comp.glsl",
                                                 "internal/rt_diffuse_shade@HIT_FIRST;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_diffuse_shade_[5] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_diffuse_shade@HIT_FIRST;GI_CACHE;STOCH_LIGHTS.comp.glsl",
                        "internal/rt_diffuse_shade@HIT_FIRST;GI_CACHE;STOCH_LIGHTS;NO_SUBGROUP.comp.glsl"));
    pi_diffuse_shade_[6] = sh_.FindOrCreatePipeline(
        subgroup_select("internal/rt_diffuse_shade@HIT_SECOND;GI_CACHE.comp.glsl",
                        "internal/rt_diffuse_shade@HIT_SECOND;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_diffuse_reproject_ = sh_.FindOrCreatePipeline("internal/rt_diffuse_reproject.comp.glsl");
    pi_diffuse_temporal_[0] = sh_.FindOrCreatePipeline("internal/rt_diffuse_temporal.comp.glsl");
    pi_diffuse_temporal_[1] = sh_.FindOrCreatePipeline("internal/rt_diffuse_temporal@RELAXED.comp.glsl");
    pi_diffuse_filter_[0] = sh_.FindOrCreatePipeline("internal/rt_diffuse_filter@PRE_FILTER.comp.glsl");
    pi_diffuse_filter_[1] = sh_.FindOrCreatePipeline("internal/rt_diffuse_filter@PRE_FILTER;RELAXED.comp.glsl");
    pi_diffuse_filter_[2] = sh_.FindOrCreatePipeline("internal/rt_diffuse_filter.comp.glsl");
    pi_diffuse_filter_[3] = sh_.FindOrCreatePipeline("internal/rt_diffuse_filter@POST_FILTER.comp.glsl");
    pi_diffuse_stabilization_ = sh_.FindOrCreatePipeline("internal/rt_diffuse_stabilization.comp.glsl");

    // Sun Shadow
    pi_sun_shadows_[0] = sh_.FindOrCreatePipeline("internal/sun_shadows@SS_SHADOW.comp.glsl");
    pi_sun_shadows_[1] = sh_.FindOrCreatePipeline("internal/sun_shadows@RT_SHADOW.comp.glsl");
    pi_sun_brightness_ = sh_.FindOrCreatePipeline("internal/sun_brightness.comp.glsl");
    pi_shadow_classify_ = sh_.FindOrCreatePipeline(subgroup_select("internal/rt_shadow_classify.comp.glsl", //
                                                                   "internal/rt_shadow_classify@NO_SUBGROUP.comp.glsl"),
                                                   32);
    pi_shadow_prepare_mask_ =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_shadow_prepare_mask.comp.glsl",
                                                 "internal/rt_shadow_prepare_mask@NO_SUBGROUP.comp.glsl"),
                                 32);
    pi_shadow_classify_tiles_ =
        sh_.FindOrCreatePipeline(subgroup_select("internal/rt_shadow_classify_tiles.comp.glsl",
                                                 "internal/rt_shadow_classify_tiles@NO_SUBGROUP.comp.glsl"),
                                 32);
    pi_shadow_filter_[0] = sh_.FindOrCreatePipeline("internal/rt_shadow_filter@PASS_0.comp.glsl");
    pi_shadow_filter_[1] = sh_.FindOrCreatePipeline("internal/rt_shadow_filter@PASS_1.comp.glsl");
    pi_shadow_filter_[2] = sh_.FindOrCreatePipeline("internal/rt_shadow_filter.comp.glsl");
    pi_shadow_debug_ = sh_.FindOrCreatePipeline("internal/rt_shadow_debug.comp.glsl");

    // Bloom
    pi_bloom_downsample_[0][0] = sh_.FindOrCreatePipeline("internal/bloom_downsample.comp.glsl");
    pi_bloom_downsample_[0][1] = sh_.FindOrCreatePipeline("internal/bloom_downsample@TONEMAP.comp.glsl");
    pi_bloom_downsample_[1][0] = sh_.FindOrCreatePipeline("internal/bloom_downsample@COMPRESSED.comp.glsl");
    pi_bloom_downsample_[1][1] = sh_.FindOrCreatePipeline("internal/bloom_downsample@COMPRESSED;TONEMAP.comp.glsl");
    pi_bloom_upsample_[0] = sh_.FindOrCreatePipeline("internal/bloom_upsample.comp.glsl");
    pi_bloom_upsample_[1] = sh_.FindOrCreatePipeline("internal/bloom_upsample@COMPRESSED.comp.glsl");

    // Autoexposure
    pi_histogram_sample_ = sh_.FindOrCreatePipeline("internal/histogram_sample.comp.glsl");
    pi_histogram_exposure_ = sh_.FindOrCreatePipeline("internal/histogram_exposure.comp.glsl");

    // Volumetrics
    pi_sky_upsample_ = sh_.FindOrCreatePipeline("internal/skydome_upsample.comp.glsl");
    pi_vol_scatter_[0][0] = sh_.FindOrCreatePipeline(subgroup_select("internal/vol_scatter.comp.glsl", //
                                                                     "internal/vol_scatter@NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[0][1] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/vol_scatter@GI_CACHE.comp.glsl", //
                                                 "internal/vol_scatter@GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[1][0] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/vol_scatter@ALL_CASCADES.comp.glsl", //
                                                 "internal/vol_scatter@ALL_CASCADES;NO_SUBGROUP.comp.glsl"));
    pi_vol_scatter_[1][1] =
        sh_.FindOrCreatePipeline(subgroup_select("internal/vol_scatter@ALL_CASCADES;GI_CACHE.comp.glsl", //
                                                 "internal/vol_scatter@ALL_CASCADES;GI_CACHE;NO_SUBGROUP.comp.glsl"));
    pi_vol_ray_march_ = sh_.FindOrCreatePipeline("internal/vol_ray_march.comp.glsl");

    // TSR
    pi_reconstruct_depth_ = sh_.FindOrCreatePipeline("internal/reconstruct_depth.comp.glsl");
    pi_prepare_disocclusion_ = sh_.FindOrCreatePipeline("internal/prepare_disocclusion.comp.glsl");
    pi_sharpen_[0] = sh_.FindOrCreatePipeline("internal/sharpen.comp.glsl");
    pi_sharpen_[1] = sh_.FindOrCreatePipeline("internal/sharpen@COMPRESSED.comp.glsl");

    // Motion blur
    pi_motion_blur_classify_[0] = sh_.FindOrCreatePipeline("internal/motion_blur_classify.comp.glsl");
    pi_motion_blur_classify_[1] = sh_.FindOrCreatePipeline("internal/motion_blur_classify@VERTICAL.comp.glsl");
    pi_motion_blur_dilate_ = sh_.FindOrCreatePipeline("internal/motion_blur_dilate.comp.glsl");
    pi_motion_blur_filter_ = sh_.FindOrCreatePipeline("internal/motion_blur_filter.comp.glsl");

    // Debugging
    pi_debug_velocity_ = sh_.FindOrCreatePipeline("internal/debug_velocity.comp.glsl");
    pi_debug_gbuffer_[0] = sh_.FindOrCreatePipeline("internal/debug_gbuffer@DEPTH.comp.glsl");
    pi_debug_gbuffer_[1] = sh_.FindOrCreatePipeline("internal/debug_gbuffer@NORMALS.comp.glsl");
    pi_debug_gbuffer_[2] = sh_.FindOrCreatePipeline("internal/debug_gbuffer@ROUGHNESS.comp.glsl");
    pi_debug_gbuffer_[3] = sh_.FindOrCreatePipeline("internal/debug_gbuffer@METALLIC.comp.glsl");

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////

    blit_static_vel_prog_ =
        sh_.FindOrCreateProgram("internal/blit_static_vel.vert.glsl", "internal/blit_static_vel.frag.glsl");
    blit_gauss_prog_ = sh_.FindOrCreateProgram("internal/blit_gauss.vert.glsl", "internal/blit_gauss.frag.glsl");
    blit_ao_prog_ = sh_.FindOrCreateProgram("internal/blit_ssao.vert.glsl", "internal/blit_ssao.frag.glsl");
    blit_bilateral_prog_ =
        sh_.FindOrCreateProgram("internal/blit_bilateral.vert.glsl", "internal/blit_bilateral.frag.glsl");
    blit_tsr_prog_ =
        sh_.FindOrCreateProgram("internal/blit_tsr.vert.glsl",
                                "internal/blit_tsr@CATMULL_ROM;ROUNDED_NEIBOURHOOD;TONEMAP;YCoCg;LOCKING.frag.glsl");
    blit_tsr_static_prog_ =
        sh_.FindOrCreateProgram("internal/blit_tsr.vert.glsl", "internal/blit_tsr@STATIC_ACCUMULATION.frag.glsl");
    blit_ssr_prog_ = sh_.FindOrCreateProgram("internal/blit_ssr.vert.glsl", "internal/blit_ssr.frag.glsl");
    blit_ssr_dilate_prog_ =
        sh_.FindOrCreateProgram("internal/blit_ssr_dilate.vert.glsl", "internal/blit_ssr_dilate.frag.glsl");
    blit_ssr_compose_prog_ =
        sh_.FindOrCreateProgram("internal/blit_ssr_compose.vert.glsl", "internal/blit_ssr_compose.frag.glsl");
    blit_upscale_prog_ = sh_.FindOrCreateProgram("internal/blit_upscale.vert.glsl", "internal/blit_upscale.frag.glsl");
    blit_down_prog_ = sh_.FindOrCreateProgram("internal/blit_down.vert.glsl", "internal/blit_down.frag.glsl");
    blit_down_depth_prog_ =
        sh_.FindOrCreateProgram("internal/blit_down_depth.vert.glsl", "internal/blit_down_depth.frag.glsl");
    blit_fxaa_prog_ = sh_.FindOrCreateProgram("internal/blit_fxaa.vert.glsl", "internal/blit_fxaa.frag.glsl");
    blit_vol_compose_prog_ =
        sh_.FindOrCreateProgram("internal/blit_vol_compose.vert.glsl", "internal/blit_vol_compose.frag.glsl");
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
        desc.views.push_back(Ren::eFormat::RG32UI);
        common_buffers.instance_indices = update_bufs.AddTransferOutput("Instance Indices", desc);
    }
    FgBufRWHandle shared_data_res;
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

    update_bufs.set_execute_cb([this, &common_buffers, &persistent_data, shared_data_res](const FgContext &fg) {
        const Ren::BufferHandle skin_transforms = fg.AccessRWBuffer(common_buffers.skin_transforms);
        const Ren::BufferHandle shape_keys = fg.AccessRWBuffer(common_buffers.shape_keys);
        // Ren::BufferHandle instances = fg.AccessRWBuffer(common_buffers.instances_res);
        const Ren::BufferHandle instance_indices = fg.AccessRWBuffer(common_buffers.instance_indices);
        const Ren::BufferHandle shared_data = fg.AccessRWBuffer(shared_data_res);
        const Ren::BufferHandle atomic_cnt = fg.AccessRWBuffer(common_buffers.atomic_cnt);

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), skin_transforms, 0,
                     uint32_t(p_list_->skin_transforms.size() * sizeof(skin_transform_t)),
                     p_list_->skin_transforms.data(), p_list_->skin_transforms_stage,
                     fg.backend_frame() * SkinTransformsBufChunkSize, SkinTransformsBufChunkSize, fg.cmd_buf());

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), shape_keys, 0,
                     p_list_->shape_keys_data.count * sizeof(shape_key_data_t), p_list_->shape_keys_data.data,
                     p_list_->shape_keys_stage, fg.backend_frame() * ShapeKeysBufChunkSize, ShapeKeysBufChunkSize,
                     fg.cmd_buf());

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), instance_indices, 0,
                     uint32_t(p_list_->instance_indices.size() * sizeof(Ren::Vec2i)), p_list_->instance_indices.data(),
                     p_list_->instance_indices_stage, fg.backend_frame() * InstanceIndicesBufChunkSize,
                     InstanceIndicesBufChunkSize, fg.cmd_buf());

        { // Prepare data that is shared for all instances
            shared_data_t shrd_data;

            shrd_data.view_from_world = view_state_.view_from_world = p_list_->draw_cam.view_matrix();
            shrd_data.clip_from_view = p_list_->draw_cam.proj_matrix();

            shrd_data.taa_info[0] = p_list_->draw_cam.px_offset()[0];
            shrd_data.taa_info[1] = p_list_->draw_cam.px_offset()[1];
            memcpy(&shrd_data.taa_info[2], &view_state_.frame_index, sizeof(float));
            shrd_data.taa_info[3] =
                std::tan(0.5f * p_list_->draw_cam.angle() * Ren::Pi<float>() / 180.0f) / float(view_state_.ren_res[1]);

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
            shrd_data.prev_clip_from_world_no_translation = view_state_.prev_clip_from_world_no_translation;
            shrd_data.world_from_view = Inverse(shrd_data.view_from_world);
            shrd_data.view_from_clip = Inverse(shrd_data.clip_from_view);
            shrd_data.world_from_clip = Inverse(shrd_data.clip_from_world);
            shrd_data.world_from_clip_no_translation = Inverse(view_state_.clip_from_world_no_translation);
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
            if (p_list_->env.env_map_name == "physical_sky") {
                // Ignore rotation
                shrd_data.env_col[3] = 0.0f;
            }

            // render resolution and full resolution
            shrd_data.ren_res = Ren::Vec4f{float(view_state_.ren_res[0]), float(view_state_.ren_res[1]),
                                           1.0f / float(view_state_.ren_res[0]), 1.0f / float(view_state_.ren_res[1])};
            shrd_data.ires_and_ifres = Ren::Vec4i{view_state_.ren_res[0], view_state_.ren_res[1],
                                                  view_state_.out_res[0], view_state_.out_res[1]};
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
                const auto &[env_main, env_cold] = ctx_.storages().images[p_list_->env.env_map];
                env_mip_count = float(env_cold.params.mip_count);
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

            UpdateBuffer(fg.ren_ctx().api(), fg.storages(), shared_data, 0, sizeof(shared_data_t), &shrd_data,
                         p_list_->shared_data_stage, fg.backend_frame() * SharedDataBlockSize, SharedDataBlockSize,
                         fg.cmd_buf());
        }

        Ren::BufferMain &atomic_cnt_buf_main = fg.storages().buffers[atomic_cnt].first;
        Buffer_Fill(fg.ren_ctx().api(), atomic_cnt_buf_main, 0, sizeof(uint32_t), 0, fg.cmd_buf());
    });
}

void Eng::Renderer::AddLightBuffersUpdatePass(CommonBuffers &common_buffers) {
    auto &update_light_bufs = fg_builder_.AddNode("UPDATE LBUFFERS");

    { // create cells buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RG32UI);
        common_buffers.cells = update_light_bufs.AddTransferOutput("Cells Buffer", desc);
    }
    { // create RT cells buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RG32UI);
        common_buffers.rt_cells = update_light_bufs.AddTransferOutput("RT Cells Buffer", desc);
    }
    { // create lights buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RGBA32UI);
        common_buffers.lights = update_light_bufs.AddTransferOutput("Lights Buffer", desc);
    }
    { // create decals buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RGBA32F);
        common_buffers.decals = update_light_bufs.AddTransferOutput("Decals Buffer", desc);
    }
    { // create items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RG32UI);
        common_buffers.items = update_light_bufs.AddTransferOutput("Items Buffer", desc);
    }
    { // create RT items buffer
        FgBufDesc desc = {};
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        desc.views.push_back(Ren::eFormat::RG32UI);
        common_buffers.rt_items = update_light_bufs.AddTransferOutput("RT Items Buffer", desc);
    }

    update_light_bufs.set_execute_cb([this, &common_buffers](const FgContext &fg) {
        const Ren::BufferHandle cells = fg.AccessRWBuffer(common_buffers.cells);
        const Ren::BufferHandle rt_cells = fg.AccessRWBuffer(common_buffers.rt_cells);
        const Ren::BufferHandle lights = fg.AccessRWBuffer(common_buffers.lights);
        const Ren::BufferHandle decals = fg.AccessRWBuffer(common_buffers.decals);
        const Ren::BufferHandle items = fg.AccessRWBuffer(common_buffers.items);
        const Ren::BufferHandle rt_items = fg.AccessRWBuffer(common_buffers.rt_items);

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), cells, 0, p_list_->cells.count * sizeof(cell_data_t),
                     p_list_->cells.data, p_list_->cells_stage, fg.backend_frame() * CellsBufChunkSize,
                     CellsBufChunkSize, fg.cmd_buf());

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), rt_cells, 0, p_list_->rt_cells.count * sizeof(cell_data_t),
                     p_list_->rt_cells.data, p_list_->rt_cells_stage, fg.backend_frame() * CellsBufChunkSize,
                     CellsBufChunkSize, fg.cmd_buf());

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), lights, 0,
                     uint32_t(p_list_->lights.size() * sizeof(light_item_t)), p_list_->lights.data(),
                     p_list_->lights_stage, fg.backend_frame() * LightsBufChunkSize, LightsBufChunkSize, fg.cmd_buf());

        UpdateBuffer(fg.ren_ctx().api(), fg.storages(), decals, 0,
                     uint32_t(p_list_->decals.size() * sizeof(decal_item_t)), p_list_->decals.data(),
                     p_list_->decals_stage, fg.backend_frame() * DecalsBufChunkSize, DecalsBufChunkSize, fg.cmd_buf());

        if (p_list_->items.count) {
            UpdateBuffer(fg.ren_ctx().api(), fg.storages(), items, 0, p_list_->items.count * sizeof(item_data_t),
                         p_list_->items.data, p_list_->items_stage, fg.backend_frame() * ItemsBufChunkSize,
                         ItemsBufChunkSize, fg.cmd_buf());
        } else {
            static const item_data_t dummy = {};
            UpdateBuffer(fg.ren_ctx().api(), fg.storages(), items, 0, sizeof(item_data_t), &dummy, p_list_->items_stage,
                         fg.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize, fg.cmd_buf());
        }

        if (p_list_->rt_items.count) {
            UpdateBuffer(fg.ren_ctx().api(), fg.storages(), rt_items, 0, p_list_->rt_items.count * sizeof(item_data_t),
                         p_list_->rt_items.data, p_list_->rt_items_stage, fg.backend_frame() * ItemsBufChunkSize,
                         ItemsBufChunkSize, fg.cmd_buf());
        } else {
            const item_data_t dummy = {};
            UpdateBuffer(fg.ren_ctx().api(), fg.storages(), rt_items, 0, sizeof(item_data_t), &dummy,
                         p_list_->rt_items_stage, fg.backend_frame() * ItemsBufChunkSize, ItemsBufChunkSize,
                         fg.cmd_buf());
        }
    });
}

void Eng::Renderer::AddGBufferFillPass(const CommonBuffers &common_buffers, const BindlessTextureData &bindless,
                                       FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

    auto &gbuf_fill = fg_builder_.AddNode("GBUFFER FILL");
    const FgBufROHandle vtx_buf1 = gbuf_fill.AddVertexBufferInput(common_buffers.vertex_buf1);
    const FgBufROHandle vtx_buf2 = gbuf_fill.AddVertexBufferInput(common_buffers.vertex_buf2);
    const FgBufROHandle ndx_buf = gbuf_fill.AddIndexBufferInput(common_buffers.indices_buf);

    const FgBufROHandle materials = gbuf_fill.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);

    const FgImgROHandle noise =
        gbuf_fill.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgImgROHandle dummy_white = gbuf_fill.AddTextureInput(frame_textures.dummy_white, Stg::FragmentShader);
    const FgImgROHandle dummy_black = gbuf_fill.AddTextureInput(frame_textures.dummy_black, Stg::FragmentShader);

    const FgBufROHandle instances = gbuf_fill.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
    const FgBufROHandle instances_indices =
        gbuf_fill.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgBufROHandle shared_data = gbuf_fill.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    const FgBufROHandle cells = gbuf_fill.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgBufROHandle items = gbuf_fill.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgBufROHandle decals = gbuf_fill.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    frame_textures.albedo = gbuf_fill.AddColorOutput(MAIN_ALBEDO_TEX, frame_textures.albedo_desc);
    frame_textures.normal = gbuf_fill.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_desc);
    frame_textures.specular = gbuf_fill.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_desc);
    frame_textures.depth = gbuf_fill.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);

    gbuf_fill.make_executor<ExGBufferFill>(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials, &bindless,
                                           noise, dummy_white, dummy_black, instances, instances_indices, shared_data,
                                           cells, items, decals, frame_textures.albedo, frame_textures.normal,
                                           frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddForwardOpaquePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                         const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

    auto &opaque = fg_builder_.AddNode("OPAQUE");
    const FgBufROHandle vtx_buf1 = opaque.AddVertexBufferInput(common_buffers.vertex_buf1);
    const FgBufROHandle vtx_buf2 = opaque.AddVertexBufferInput(common_buffers.vertex_buf2);
    const FgBufROHandle ndx_buf = opaque.AddIndexBufferInput(common_buffers.indices_buf);

    const FgBufROHandle materials = opaque.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);
    const FgImgROHandle brdf_lut = opaque.AddTextureInput(frame_textures.brdf_lut, Stg::FragmentShader);
    const FgImgROHandle noise =
        opaque.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgImgROHandle cone_rt_lut = opaque.AddTextureInput(frame_textures.cone_rt_lut, Stg::FragmentShader);

    const FgImgROHandle dummy_black = opaque.AddTextureInput(frame_textures.dummy_black, Stg::FragmentShader);

    const FgBufROHandle instances = opaque.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
    const FgBufROHandle instances_indices =
        opaque.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgBufROHandle shader_data =
        opaque.AddUniformBufferInput(common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    const FgBufROHandle cells = opaque.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgBufROHandle items = opaque.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgBufROHandle lights = opaque.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
    const FgBufROHandle decals = opaque.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    const FgImgROHandle shadow_depth = opaque.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
    const FgImgROHandle ssao = opaque.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

    FgResRef lmap[4];
    for (int i = 0; i < 4; ++i) {
        if (p_list_->env.lm_indir_sh[i]) {
            // lmap[i] = opaque.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
        }
    }

    frame_textures.color = opaque.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);
    frame_textures.normal = opaque.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_desc);
    frame_textures.specular = opaque.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_desc);
    frame_textures.depth = opaque.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);

    opaque.make_executor<ExOpaque>(
        ctx_.api(), &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials, &bindless, brdf_lut, noise,
        cone_rt_lut, dummy_black, instances, instances_indices, shader_data, cells, items, lights, decals, shadow_depth,
        ssao, lmap, frame_textures.color, frame_textures.normal, frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddForwardTransparentPass(const CommonBuffers &common_buffers,
                                              const PersistentGpuData &persistent_data,
                                              const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

    auto &transparent = fg_builder_.AddNode("TRANSPARENT");
    const FgBufROHandle vtx_buf1 = transparent.AddVertexBufferInput(common_buffers.vertex_buf1);
    const FgBufROHandle vtx_buf2 = transparent.AddVertexBufferInput(common_buffers.vertex_buf2);
    const FgBufROHandle ndx_buf = transparent.AddIndexBufferInput(common_buffers.indices_buf);

    const FgBufROHandle materials = transparent.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);
    const FgImgROHandle brdf_lut = transparent.AddTextureInput(frame_textures.brdf_lut, Stg::FragmentShader);
    const FgImgROHandle noise =
        transparent.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgImgROHandle cone_rt_lut = transparent.AddTextureInput(frame_textures.cone_rt_lut, Stg::FragmentShader);

    const FgImgROHandle dummy_black = transparent.AddTextureInput(frame_textures.dummy_black, Stg::FragmentShader);

    const FgBufROHandle instances = transparent.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
    const FgBufROHandle instances_indices =
        transparent.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgBufROHandle shader_data = transparent.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    const FgBufROHandle cells = transparent.AddStorageReadonlyInput(common_buffers.cells, Stg::FragmentShader);
    const FgBufROHandle items = transparent.AddStorageReadonlyInput(common_buffers.items, Stg::FragmentShader);
    const FgBufROHandle lights = transparent.AddStorageReadonlyInput(common_buffers.lights, Stg::FragmentShader);
    const FgBufROHandle decals = transparent.AddStorageReadonlyInput(common_buffers.decals, Stg::FragmentShader);

    const FgImgROHandle shadow_depth = transparent.AddTextureInput(frame_textures.shadow_depth, Stg::FragmentShader);
    const FgImgROHandle ssao = transparent.AddTextureInput(frame_textures.ssao, Stg::FragmentShader);

    FgResRef lmap[4];
    for (int i = 0; i < 4; ++i) {
        if (p_list_->env.lm_indir_sh[i]) {
            // lmap[i] = transparent.AddTextureInput(p_list_->env.lm_indir_sh[i], Stg::FragmentShader);
        }
    }

    frame_textures.color = transparent.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);
    frame_textures.normal = transparent.AddColorOutput(MAIN_NORMAL_TEX, frame_textures.normal_desc);
    frame_textures.specular = transparent.AddColorOutput(MAIN_SPEC_TEX, frame_textures.specular_desc);
    frame_textures.depth = transparent.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);

    transparent.make_executor<ExTransparent>(
        ctx_.api(), &p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials, &bindless, brdf_lut, noise,
        cone_rt_lut, dummy_black, instances, instances_indices, shader_data, cells, items, lights, decals, shadow_depth,
        ssao, lmap, frame_textures.color, frame_textures.normal, frame_textures.specular, frame_textures.depth);
}

void Eng::Renderer::AddDeferredShadingPass(const CommonBuffers &common_buffers, FrameTextures &frame_textures,
                                           const bool enable_gi) {
    using Stg = Ren::eStage;
    using Trg = Ren::eBindTarget;

    auto &gbuf_shade = fg_builder_.AddNode("GBUFFER SHADE");

    struct PassData {
        FgBufROHandle shared_data;
        FgBufROHandle cells, items, lights, decals;
        FgImgROHandle shadow_depth, shadow_color;
        FgImgROHandle ssao;
        FgImgROHandle gi;
        FgImgROHandle sun_shadow;
        FgImgROHandle depth, albedo, normal, spec;
        FgImgROHandle env;
        FgImgROHandle ltc_luts;
        FgImgRWHandle output;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->shared_data = gbuf_shade.AddUniformBufferInput(common_buffers.shared_data, Stg::ComputeShader);

    data->cells = gbuf_shade.AddStorageReadonlyInput(common_buffers.cells, Stg::ComputeShader);
    data->items = gbuf_shade.AddStorageReadonlyInput(common_buffers.items, Stg::ComputeShader);
    data->lights = gbuf_shade.AddStorageReadonlyInput(common_buffers.lights, Stg::ComputeShader);
    data->decals = gbuf_shade.AddStorageReadonlyInput(common_buffers.decals, Stg::ComputeShader);

    data->shadow_depth = gbuf_shade.AddTextureInput(frame_textures.shadow_depth, Stg::ComputeShader);
    data->shadow_color = gbuf_shade.AddTextureInput(frame_textures.shadow_color, Stg::ComputeShader);
    data->ssao = gbuf_shade.AddTextureInput(frame_textures.ssao, Stg::ComputeShader);
    if (enable_gi) {
        data->gi = gbuf_shade.AddTextureInput(frame_textures.gi_diffuse, Stg::ComputeShader);
    } else {
        data->gi = gbuf_shade.AddTextureInput(frame_textures.dummy_black, Stg::ComputeShader);
    }
    data->sun_shadow = gbuf_shade.AddTextureInput(frame_textures.sun_shadow, Stg::ComputeShader);

    data->depth = gbuf_shade.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
    data->albedo = gbuf_shade.AddTextureInput(frame_textures.albedo, Stg::ComputeShader);
    data->normal = gbuf_shade.AddTextureInput(frame_textures.normal, Stg::ComputeShader);
    data->spec = gbuf_shade.AddTextureInput(frame_textures.specular, Stg::ComputeShader);

    data->ltc_luts = gbuf_shade.AddTextureInput(frame_textures.ltc_luts, Stg::ComputeShader);
    data->env = gbuf_shade.AddTextureInput(frame_textures.envmap, Stg::ComputeShader);

    frame_textures.color = data->output =
        gbuf_shade.AddStorageImageOutput(MAIN_COLOR_TEX, frame_textures.color_desc, Stg::ComputeShader);

    gbuf_shade.set_execute_cb([this, data](const FgContext &fg) {
        using namespace GBufferShade;

        const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(data->shared_data);
        const Ren::BufferROHandle cells = fg.AccessROBuffer(data->cells);
        const Ren::BufferROHandle items = fg.AccessROBuffer(data->items);
        const Ren::BufferROHandle lights = fg.AccessROBuffer(data->lights);
        const Ren::BufferROHandle decals = fg.AccessROBuffer(data->decals);

        const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
        const Ren::ImageROHandle albedo = fg.AccessROImage(data->albedo);
        const Ren::ImageROHandle normal = fg.AccessROImage(data->normal);
        const Ren::ImageROHandle spec = fg.AccessROImage(data->spec);

        const Ren::ImageROHandle shadow_depth = fg.AccessROImage(data->shadow_depth);
        const Ren::ImageROHandle shadow_color = fg.AccessROImage(data->shadow_color);
        const Ren::ImageROHandle ssao = fg.AccessROImage(data->ssao);
        const Ren::ImageROHandle gi = fg.AccessROImage(data->gi);
        const Ren::ImageROHandle sun_shadow = fg.AccessROImage(data->sun_shadow);

        const Ren::ImageROHandle ltc_luts = fg.AccessROImage(data->ltc_luts);
        const Ren::ImageROHandle env = fg.AccessROImage(data->env);

        const Ren::ImageRWHandle out_color = fg.AccessRWImage(data->output);

        const Ren::Binding bindings[] = {{Trg::UBuf, BIND_UB_SHARED_DATA_BUF, unif_shared_data},
                                         {Trg::UTBuf, CELLS_BUF_SLOT, cells},
                                         {Trg::UTBuf, ITEMS_BUF_SLOT, items},
                                         {Trg::UTBuf, LIGHT_BUF_SLOT, lights},
                                         {Trg::UTBuf, DECAL_BUF_SLOT, decals},
                                         {Trg::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                         {Trg::TexSampled, DEPTH_LIN_TEX_SLOT, {depth, linear_sampler_, 1}},
                                         {Trg::TexSampled, ALBEDO_TEX_SLOT, albedo},
                                         {Trg::TexSampled, NORM_TEX_SLOT, normal},
                                         {Trg::TexSampled, SPEC_TEX_SLOT, spec},
                                         {Trg::TexSampled, SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                                         {Trg::TexSampled, SHADOW_DEPTH_VAL_TEX_SLOT, {shadow_depth, nearest_sampler_}},
                                         {Trg::TexSampled, SHADOW_COLOR_TEX_SLOT, shadow_color},
                                         {Trg::TexSampled, SSAO_TEX_SLOT, ssao},
                                         {Trg::TexSampled, GI_TEX_SLOT, gi},
                                         {Trg::TexSampled, SUN_SHADOW_TEX_SLOT, sun_shadow},
                                         {Trg::TexSampled, LTC_LUTS_TEX_SLOT, ltc_luts},
                                         {Trg::TexSampled, ENV_TEX_SLOT, env},
                                         {Trg::ImageRW, OUT_COLOR_IMG_SLOT, out_color}};

        const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_.ren_res[0], GRP_SIZE_X),
                                          Ren::DivCeil(view_state_.ren_res[1], GRP_SIZE_Y), 1u);

        Params uniform_params;
        uniform_params.img_size = Ren::Vec2u(view_state_.ren_res[0], view_state_.ren_res[1]);
        uniform_params.pixel_spread_angle = view_state_.pixel_spread_angle;

        DispatchCompute(fg.cmd_buf(), pi_gbuf_shade_[settings.enable_shadow_jitter], fg.storages(), grp_count, bindings,
                        &uniform_params, sizeof(uniform_params), fg.descr_alloc(), fg.log());
    });
}

void Eng::Renderer::AddEmissivePass(const CommonBuffers &common_buffers, const PersistentGpuData &persistent_data,
                                    const BindlessTextureData &bindless, FrameTextures &frame_textures) {
    using Stg = Ren::eStage;

    auto &emissive = fg_builder_.AddNode("EMISSIVE");

    const FgBufROHandle vtx_buf1 = emissive.AddVertexBufferInput(common_buffers.vertex_buf1);
    const FgBufROHandle vtx_buf2 = emissive.AddVertexBufferInput(common_buffers.vertex_buf2);
    const FgBufROHandle ndx_buf = emissive.AddIndexBufferInput(common_buffers.indices_buf);

    const FgBufROHandle materials = emissive.AddStorageReadonlyInput(common_buffers.materials, Stg::VertexShader);

    const FgImgROHandle noise =
        emissive.AddTextureInput(frame_textures.noise, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    const FgImgROHandle dummy_white = emissive.AddTextureInput(frame_textures.dummy_white, Stg::FragmentShader);

    const FgBufROHandle instances = emissive.AddStorageReadonlyInput(common_buffers.instances, Stg::VertexShader);
    const FgBufROHandle instances_indices =
        emissive.AddStorageReadonlyInput(common_buffers.instance_indices, Stg::VertexShader);

    const FgBufROHandle shared_data = emissive.AddUniformBufferInput(
        common_buffers.shared_data, Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);

    frame_textures.color = emissive.AddColorOutput(MAIN_COLOR_TEX, frame_textures.color_desc);
    frame_textures.depth = emissive.AddDepthOutput(MAIN_DEPTH_TEX, frame_textures.depth_desc);

    emissive.make_executor<ExEmissive>(&p_list_, &view_state_, vtx_buf1, vtx_buf2, ndx_buf, materials, &bindless, noise,
                                       dummy_white, instances, instances_indices, shared_data, frame_textures.color,
                                       frame_textures.depth);
}

void Eng::Renderer::AddFillStaticVelocityPass(const CommonBuffers &common_buffers, const FgImgRWHandle depth_tex,
                                              FgImgRWHandle &inout_velocity) {
    using Stg = Ren::eStage;

    auto &static_vel = fg_builder_.AddNode("FILL STATIC VEL");

    struct PassData {
        FgBufROHandle shared_data;
        FgImgROHandle in_depth;

        FgImgRWHandle out_velocity;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->shared_data = static_vel.AddUniformBufferInput(common_buffers.shared_data,
                                                         Ren::Bitmask{Stg::VertexShader} | Stg::FragmentShader);
    data->in_depth = static_vel.AddCustomTextureInput(depth_tex, Ren::eResState::StencilTestDepthFetch,
                                                      Ren::Bitmask{Stg::DepthAttachment} | Stg::FragmentShader);
    inout_velocity = data->out_velocity = static_vel.AddColorOutput(inout_velocity);

    static_vel.set_execute_cb([this, data](const FgContext &fg) {
        const Ren::BufferROHandle unif_shared_data = fg.AccessROBuffer(data->shared_data);
        const Ren::ImageROHandle in_depth = fg.AccessROImage(data->in_depth);

        const Ren::ImageRWHandle out_velocity = fg.AccessRWImage(data->out_velocity);

        Ren::RastState rast_state;
        rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);
        rast_state.stencil.enabled = true;
        rast_state.stencil.write_mask = 0x00;
        rast_state.stencil.compare_op = unsigned(Ren::eCompareOp::Equal);

        rast_state.viewport[2] = view_state_.ren_res[0];
        rast_state.viewport[3] = view_state_.ren_res[1];

        const Ren::Binding bindings[] = {
            {Ren::eBindTarget::TexSampled, BlitStaticVel::DEPTH_TEX_SLOT, {in_depth, 1}},
            {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, 0, sizeof(shared_data_t), unif_shared_data}};

        // TODO: Make this cleaner!
        Ren::ImageRWHandle depth_rw_handle = {in_depth.index, in_depth.generation};

        const Ren::RenderTarget render_targets[] = {{out_velocity, Ren::eLoadOp::Load, Ren::eStoreOp::Store}};
        const Ren::RenderTarget depth_target = {depth_rw_handle, Ren::eLoadOp::None, Ren::eStoreOp::None,
                                                Ren::eLoadOp::Load, Ren::eStoreOp::None};

        BlitStaticVel::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_.ren_res[0]), float(view_state_.ren_res[1])};

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_static_vel_prog_, depth_target, render_targets,
                            rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(BlitStaticVel::Params), 0,
                            fg.framebuffers());
    });
}

Eng::FgImgRWHandle Eng::Renderer::AddTSRPasses(const CommonBuffers &common_buffers, FrameTextures &frame_textures,
                                               const bool static_accumulation) {
    using Stg = Ren::eStage;

    FgImgRWHandle reconstructed_depth;
    { // Clear reconstructed depth
        auto &clear_reconstructed = fg_builder_.AddNode("CLEAR REC. DEPTH");

        struct PassData {
            FgImgRWHandle reconstructed_depth;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        { // Reconstructed previous depth
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R32UI;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::ClampToBorder;

            reconstructed_depth = data->reconstructed_depth =
                clear_reconstructed.AddClearImageOutput("Reconstructed Prev. Depth", desc);
        }

        clear_reconstructed.set_execute_cb([data](const FgContext &fg) {
            const Ren::ImageRWHandle reconstructed_depth = fg.AccessRWImage(data->reconstructed_depth);

            fg.ren_ctx().CmdClearImage(reconstructed_depth, {}, fg.cmd_buf());
        });
    }

    FgImgRWHandle dilated_depth, dilated_velocity;
    { // Reconstruct previous depth
        auto &reconstruct = fg_builder_.AddNode("RECONSTRUCT DEPTH");

        struct PassData {
            FgImgROHandle depth;
            FgImgROHandle velocity;

            FgImgRWHandle out_reconstructed_depth;
            FgImgRWHandle out_dilated_depth;
            FgImgRWHandle out_dilated_velocity;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->depth = reconstruct.AddTextureInput(frame_textures.depth, Stg::ComputeShader);
        data->velocity = reconstruct.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);

        reconstructed_depth = data->out_reconstructed_depth =
            reconstruct.AddStorageImageOutput(reconstructed_depth, Stg::ComputeShader);
        { // Dilated current depth
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::R32F;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::ClampToBorder;

            dilated_depth = data->out_dilated_depth =
                reconstruct.AddStorageImageOutput("Dilated Depth", desc, Stg::ComputeShader);
        }
        { // Dilated motion
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG16F;
            desc.sampling.filter = Ren::eFilter::Nearest;
            desc.sampling.wrap = Ren::eWrap::ClampToBorder;

            dilated_velocity = data->out_dilated_velocity =
                reconstruct.AddStorageImageOutput("Dilated Velocity", desc, Stg::ComputeShader);
        }

        reconstruct.set_execute_cb([this, data](const FgContext &fg) {
            using namespace ReconstructDepth;

            const Ren::ImageROHandle depth = fg.AccessROImage(data->depth);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);

            const Ren::ImageRWHandle reconstructed_depth = fg.AccessRWImage(data->out_reconstructed_depth);
            const Ren::ImageRWHandle dilated_depth = fg.AccessRWImage(data->out_dilated_depth);
            const Ren::ImageRWHandle dilated_velocity = fg.AccessRWImage(data->out_dilated_velocity);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                {Ren::eBindTarget::TexSampled, VELOCITY_TEX_SLOT, velocity},
                {Ren::eBindTarget::ImageRW, OUT_RECONSTRUCTED_DEPTH_IMG_SLOT, reconstructed_depth},
                {Ren::eBindTarget::ImageRW, OUT_DILATED_DEPTH_IMG_SLOT, dilated_depth},
                {Ren::eBindTarget::ImageRW, OUT_DILATED_VELOCITY_IMG_SLOT, dilated_velocity}};

            const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_.ren_res[0], GRP_SIZE_X),
                                              Ren::DivCeil(view_state_.ren_res[1], GRP_SIZE_Y), 1u);

            Params uniform_params;
            uniform_params.img_size = view_state_.ren_res;
            uniform_params.texel_size = 1.0f / Ren::Vec2f(view_state_.ren_res);

            DispatchCompute(fg.cmd_buf(), pi_reconstruct_depth_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    FgImgRWHandle disocclusion_mask;
    { // Prepare disocclusion
        auto &prep_disocclusion = fg_builder_.AddNode("PREP DISOCCLUSION");

        struct PassData {
            FgImgROHandle dilated_depth;
            FgImgROHandle dilated_velocity;
            FgImgROHandle reconstructed_depth;
            FgImgROHandle velocity;

            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->dilated_depth = prep_disocclusion.AddTextureInput(dilated_depth, Stg::ComputeShader);
        data->dilated_velocity = prep_disocclusion.AddTextureInput(dilated_velocity, Stg::ComputeShader);
        data->reconstructed_depth = prep_disocclusion.AddTextureInput(reconstructed_depth, Stg::ComputeShader);
        data->velocity = prep_disocclusion.AddTextureInput(frame_textures.velocity, Stg::ComputeShader);

        { // Image that holds disocclusion
            FgImgDesc desc;
            desc.w = view_state_.ren_res[0];
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RG8;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToBorder;

            disocclusion_mask = data->output =
                prep_disocclusion.AddStorageImageOutput("Disocclusion Mask", desc, Stg::ComputeShader);
        }

        prep_disocclusion.set_execute_cb([this, data](const FgContext &fg) {
            using namespace PrepareDisocclusion;

            const Ren::ImageROHandle dilated_depth = fg.AccessROImage(data->dilated_depth);
            const Ren::ImageROHandle dilated_velocity = fg.AccessROImage(data->dilated_velocity);
            const Ren::ImageROHandle reconstructed_depth = fg.AccessROImage(data->reconstructed_depth);
            const Ren::ImageROHandle velocity = fg.AccessROImage(data->velocity);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, DILATED_DEPTH_TEX_SLOT, dilated_depth},
                {Ren::eBindTarget::TexSampled, DILATED_VELOCITY_TEX_SLOT, dilated_velocity},
                {Ren::eBindTarget::TexSampled, RECONSTRUCTED_DEPTH_TEX_SLOT, reconstructed_depth},
                {Ren::eBindTarget::TexSampled, VELOCITY_TEX_SLOT, velocity},
                {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, output}};

            const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_.ren_res[0], GRP_SIZE_X),
                                              Ren::DivCeil(view_state_.ren_res[1], GRP_SIZE_Y), 1u);

            Params uniform_params;
            uniform_params.img_size = view_state_.ren_res;
            uniform_params.texel_size = 1.0f / Ren::Vec2f(view_state_.ren_res);
            uniform_params.clip_info = view_state_.clip_info;
            uniform_params.frustum_info = view_state_.frustum_info;

            DispatchCompute(fg.cmd_buf(), pi_prepare_disocclusion_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    FgImgRWHandle resolved_color;
    { // Main TSR pass
        auto &taa = fg_builder_.AddNode("TSR");

        struct PassData {
            FgImgROHandle color;
            FgImgROHandle history;
            FgImgROHandle dilated_depth;
            FgImgROHandle dilated_velocity;
            FgImgROHandle disocclusion_mask;

            FgImgRWHandle output;
            FgImgRWHandle output_history;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->color = taa.AddTextureInput(frame_textures.color, Stg::FragmentShader);
        data->dilated_depth = taa.AddTextureInput(dilated_depth, Stg::FragmentShader);
        data->dilated_velocity = taa.AddTextureInput(dilated_velocity, Stg::FragmentShader);
        data->disocclusion_mask = taa.AddTextureInput(disocclusion_mask, Stg::FragmentShader);

        { // Image that holds resolved color
            FgImgDesc desc;
            desc.w = view_state_.out_res[0];
            desc.h = view_state_.out_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToBorder;

            resolved_color = data->output = taa.AddColorOutput("Resolved Color", desc);
            data->output_history = taa.AddColorOutput("Color History", desc);
        }
        data->history = taa.AddHistoryTextureInput(data->output_history, Stg::FragmentShader);

        taa.set_execute_cb([this, data, static_accumulation](const FgContext &fg) {
            const Ren::ImageROHandle color = fg.AccessROImage(data->color);
            const Ren::ImageROHandle history = fg.AccessROImage(data->history);
            const Ren::ImageROHandle dilated_depth = fg.AccessROImage(data->dilated_depth);
            const Ren::ImageROHandle dilated_velocity = fg.AccessROImage(data->dilated_velocity);
            const Ren::ImageROHandle disocclusion_mask = fg.AccessROImage(data->disocclusion_mask);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);
            const Ren::ImageRWHandle output_history = fg.AccessRWImage(data->output_history);

            Ren::RastState rast_state;
            rast_state.poly.cull = uint8_t(Ren::eCullFace::Back);

            rast_state.viewport[2] = view_state_.out_res[0];
            rast_state.viewport[3] = view_state_.out_res[1];

            const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store},
                                                        {output_history, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

            const Ren::Binding bindings[] = {
                {Ren::eBindTarget::TexSampled, TSR::CURR_NEAREST_TEX_SLOT, {color, nearest_sampler_}},
                {Ren::eBindTarget::TexSampled, TSR::CURR_LINEAR_TEX_SLOT, color},
                {Ren::eBindTarget::TexSampled, TSR::HIST_TEX_SLOT, history},
                {Ren::eBindTarget::TexSampled, TSR::DILATED_DEPTH_TEX_SLOT, dilated_depth},
                {Ren::eBindTarget::TexSampled, TSR::DILATED_VELOCITY_TEX_SLOT, dilated_velocity},
                {Ren::eBindTarget::TexSampled, TSR::DISOCCLUSION_MASK_TEX_SLOT, disocclusion_mask}};

            TSR::Params uniform_params;
            uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, view_state_.out_res[0], view_state_.out_res[1]};
            uniform_params.texel_size = 1.0f / Ren::Vec4f(float(view_state_.ren_res[0]), float(view_state_.ren_res[1]),
                                                          float(view_state_.out_res[0]), float(view_state_.out_res[1]));
            uniform_params.unjitter = Ren::Vec2f{view_state_.jitter[0] - 0.5f, view_state_.jitter[1] - 0.5f};
            uniform_params.significant_change =
                Dot(p_list_->env.sun_dir, view_state_.prev_sun_dir) < 0.99999f ? 1.0f : 0.0f;
            if (static_accumulation && int(accumulated_frames_) < RendererInternal::TaaSampleCountStatic) {
                uniform_params.mix_factor = 1.0f / (1.0f + accumulated_frames_);
            } else {
                uniform_params.mix_factor = 0.0f;
            }
            uniform_params.downscale_factor = 1.0f / p_list_->render_settings.resolution_scale;
            ++accumulated_frames_;

            prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad,
                                static_accumulation ? blit_tsr_static_prog_ : blit_tsr_prog_, {}, render_targets,
                                rast_state, fg.rast_state(), bindings, &uniform_params, sizeof(TSR::Params), 0,
                                fg.framebuffers());
        });
    }
    frame_textures.disocclusion_mask = disocclusion_mask;
    return resolved_color;
}

Eng::FgImgRWHandle Eng::Renderer::AddMotionBlurPasses(FgImgROHandle input, FrameTextures &frame_textures) {
    FgImgRWHandle tiles;
    { // Horizontal pass
        auto &classify_h = fg_builder_.AddNode("MB CLASSIFY H");

        struct PassData {
            FgImgROHandle in_velocity;
            FgImgRWHandle out_tiles;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_velocity = classify_h.AddTextureInput(frame_textures.velocity, Ren::eStage::ComputeShader);

        { // Image that holds horizontal tiles
            FgImgDesc desc;
            desc.w = Ren::DivCeil(view_state_.ren_res[0], MotionBlur::TILE_RES);
            desc.h = view_state_.ren_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            tiles = data->out_tiles = classify_h.AddStorageImageOutput("MB Tiles H", desc, Ren::eStage::ComputeShader);
        }

        classify_h.set_execute_cb([this, data](const FgContext &fg) {
            const Ren::ImageROHandle in_velocity = fg.AccessROImage(data->in_velocity);
            const Ren::ImageRWHandle out_tiles = fg.AccessRWImage(data->out_tiles);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, MotionBlur::VELOCITY_TEX_SLOT, in_velocity},
                                             {Ren::eBindTarget::ImageRW, MotionBlur::OUT_IMG_SLOT, out_tiles}};

            // TODO: Get rid of this!
            const Ren::ImageCold &img_cold = fg.storages().images[in_velocity].second;
            const auto grp_count =
                Ren::Vec3u{Ren::DivCeil<uint32_t>(img_cold.params.w, MotionBlur::GRP_SIZE), img_cold.params.h, 1u};

            MotionBlur::Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{img_cold.params.w, img_cold.params.h};

            DispatchCompute(fg.cmd_buf(), pi_motion_blur_classify_[0], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    { // Vertical pass
        auto &classify_v = fg_builder_.AddNode("MB CLASSIFY V");

        struct PassData {
            FgImgROHandle in_velocity;
            FgImgRWHandle out_tiles;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_velocity = classify_v.AddTextureInput(tiles, Ren::eStage::ComputeShader);

        { // Image that holds final tiles
            FgImgDesc desc;
            desc.w = Ren::DivCeil(view_state_.ren_res[0], MotionBlur::TILE_RES);
            desc.h = Ren::DivCeil(view_state_.ren_res[1], MotionBlur::TILE_RES);
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            tiles = data->out_tiles = classify_v.AddStorageImageOutput("MB Tiles V", desc, Ren::eStage::ComputeShader);
        }

        classify_v.set_execute_cb([this, data](const FgContext &fg) {
            using namespace MotionBlur;

            const Ren::ImageROHandle in_velocity = fg.AccessROImage(data->in_velocity);
            const Ren::ImageRWHandle out_tiles = fg.AccessRWImage(data->out_tiles);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, VELOCITY_TEX_SLOT, in_velocity},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_tiles}};

            const Ren::Vec2u in_res =
                Ren::Vec2u(Ren::DivCeil(view_state_.ren_res[0], MotionBlur::TILE_RES), view_state_.ren_res[1]);

            const auto grp_count = Ren::Vec3u{in_res[0], Ren::DivCeil<uint32_t>(in_res[1], GRP_SIZE), 1u};

            Params uniform_params;
            uniform_params.img_size = in_res;

            DispatchCompute(fg.cmd_buf(), pi_motion_blur_classify_[1], fg.storages(), grp_count, bindings,
                            &uniform_params, sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    { // Dilation pass
        auto &dilate = fg_builder_.AddNode("MB DILATE");

        struct PassData {
            FgImgROHandle in_tiles;
            FgImgRWHandle out_tiles;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_tiles = dilate.AddTextureInput(tiles, Ren::eStage::ComputeShader);

        { // Image that holds dilated tiles
            FgImgDesc desc;
            desc.w = Ren::DivCeil(view_state_.ren_res[0], MotionBlur::TILE_RES);
            desc.h = Ren::DivCeil(view_state_.ren_res[1], MotionBlur::TILE_RES);
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.filter = Ren::eFilter::Bilinear;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            tiles = data->out_tiles =
                dilate.AddStorageImageOutput("MB Tiles Dilated", desc, Ren::eStage::ComputeShader);
        }

        dilate.set_execute_cb([this, data](const FgContext &fg) {
            using namespace MotionBlur;

            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);
            const Ren::ImageRWHandle out_tiles = fg.AccessRWImage(data->out_tiles);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, out_tiles}};

            const auto res = Ren::Vec2u(Ren::DivCeil(view_state_.ren_res[0], MotionBlur::TILE_RES),
                                        Ren::DivCeil(view_state_.ren_res[1], MotionBlur::TILE_RES));

            const auto grp_count = Ren::Vec3u(Ren::DivCeil(res[0], GRP_SIZE_X), Ren::DivCeil(res[1], GRP_SIZE_Y), 1u);

            Params uniform_params;
            uniform_params.img_size = res;

            DispatchCompute(fg.cmd_buf(), pi_motion_blur_dilate_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    FgImgRWHandle output;
    { // Filter pass
        auto &filter = fg_builder_.AddNode("MB FILTER");

        struct PassData {
            FgImgROHandle in_color;
            FgImgROHandle in_depth;
            FgImgROHandle in_velocity;
            FgImgROHandle in_tiles;
            FgImgRWHandle output;
        };

        auto *data = fg_builder_.AllocTempData<PassData>();
        data->in_color = filter.AddTextureInput(input, Ren::eStage::ComputeShader);
        data->in_depth = filter.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
        data->in_velocity = filter.AddTextureInput(frame_textures.velocity, Ren::eStage::ComputeShader);
        data->in_tiles = filter.AddTextureInput(tiles, Ren::eStage::ComputeShader);

        { // Image that holds filtered color
            FgImgDesc desc;
            desc.w = view_state_.out_res[0];
            desc.h = view_state_.out_res[1];
            desc.format = Ren::eFormat::RGBA16F;
            desc.sampling.wrap = Ren::eWrap::ClampToEdge;

            output = data->output = filter.AddStorageImageOutput("MB Output", desc, Ren::eStage::ComputeShader);
        }

        filter.set_execute_cb([this, data](const FgContext &fg) {
            using namespace MotionBlur;

            const Ren::ImageROHandle in_color = fg.AccessROImage(data->in_color);
            const Ren::ImageROHandle in_depth = fg.AccessROImage(data->in_depth);
            const Ren::ImageROHandle in_velocity = fg.AccessROImage(data->in_velocity);
            const Ren::ImageROHandle in_tiles = fg.AccessROImage(data->in_tiles);

            const Ren::ImageRWHandle output = fg.AccessRWImage(data->output);

            const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, COLOR_TEX_SLOT, in_color},
                                             {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {in_depth, 1}},
                                             {Ren::eBindTarget::TexSampled, VELOCITY_TEX_SLOT, in_velocity},
                                             {Ren::eBindTarget::TexSampled, TILES_TEX_SLOT, in_tiles},
                                             {Ren::eBindTarget::ImageRW, OUT_IMG_SLOT, output}};

            const Ren::ImageCold &img_cold = fg.storages().images[in_color].second;
            const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(img_cold.params.w, GRP_SIZE_X),
                                              Ren::DivCeil<uint32_t>(img_cold.params.h, GRP_SIZE_Y), 1u};

            Params uniform_params;
            uniform_params.img_size = Ren::Vec2u{img_cold.params.w, img_cold.params.h};
            uniform_params.inv_ren_res = 1.0f / Ren::Vec2f{view_state_.ren_res};
            uniform_params.clip_info = view_state_.clip_info;

            DispatchCompute(fg.cmd_buf(), pi_motion_blur_filter_, fg.storages(), grp_count, bindings, &uniform_params,
                            sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
        });
    }
    return output;
}

Eng::FgImgRWHandle Eng::Renderer::AddDownsampleDepthPass(const CommonBuffers &common_buffers, FgImgROHandle depth) {
    auto &downsample_depth = fg_builder_.AddNode("DOWN DEPTH");

    struct PassData {
        FgImgROHandle in_depth;
        FgImgRWHandle out_depth;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->in_depth = downsample_depth.AddTextureInput(depth, Ren::eStage::FragmentShader);

    FgImgRWHandle ret;
    { // Image that holds 2x downsampled linear depth
        FgImgDesc desc;
        desc.w = view_state_.ren_res[0] / 2;
        desc.h = view_state_.ren_res[1] / 2;
        desc.format = Ren::eFormat::R32F;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        ret = data->out_depth = downsample_depth.AddColorOutput("Depth Down 2X", desc);
    }

    downsample_depth.set_execute_cb([this, data](const FgContext &fg) {
        const Ren::ImageROHandle depth = fg.AccessROImage(data->in_depth);
        const Ren::ImageRWHandle output = fg.AccessRWImage(data->out_depth);

        Ren::RastState rast_state;

        rast_state.viewport[2] = view_state_.ren_res[0] / 2;
        rast_state.viewport[3] = view_state_.ren_res[1] / 2;

        const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DownDepth::DEPTH_TEX_SLOT, {depth, 1}}};

        DownDepth::Params uniform_params;
        uniform_params.transform = Ren::Vec4f{0.0f, 0.0f, float(view_state_.ren_res[0]), float(view_state_.ren_res[1])};
        uniform_params.clip_info = view_state_.clip_info;
        uniform_params.linearize = 1.0f;

        const Ren::RenderTarget render_targets[] = {{output, Ren::eLoadOp::DontCare, Ren::eStoreOp::Store}};

        prim_draw_.DrawPrim(fg.cmd_buf(), PrimDraw::ePrim::Quad, blit_down_depth_prog_, {}, render_targets, rast_state,
                            fg.rast_state(), bindings, &uniform_params, sizeof(DownDepth::Params), 0,
                            fg.framebuffers());
    });

    return ret;
}

Eng::FgImgRWHandle Eng::Renderer::AddDebugVelocityPass(const FgImgROHandle velocity) {
    auto &debug_motion = fg_builder_.AddNode("DEBUG MOTION");

    struct PassData {
        FgImgROHandle in_velocity;
        FgImgRWHandle out_color;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->in_velocity = debug_motion.AddTextureInput(velocity, Ren::eStage::ComputeShader);

    FgImgRWHandle output;
    { // Output texture
        FgImgDesc desc;
        desc.w = view_state_.out_res[0];
        desc.h = view_state_.out_res[1];
        desc.format = Ren::eFormat::RGBA8;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        output = data->out_color = debug_motion.AddStorageImageOutput("Motion Debug", desc, Ren::eStage::ComputeShader);
    }

    debug_motion.set_execute_cb([this, data](const FgContext &fg) {
        const Ren::ImageROHandle velocity = fg.AccessROImage(data->in_velocity);
        const Ren::ImageRWHandle output = fg.AccessRWImage(data->out_color);

        const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DebugVelocity::VELOCITY_TEX_SLOT, velocity},
                                         {Ren::eBindTarget::ImageRW, DebugVelocity::OUT_IMG_SLOT, output}};

        const auto grp_count =
            Ren::Vec3u{Ren::DivCeil<uint32_t>(view_state_.out_res[0], DebugVelocity::GRP_SIZE_X),
                       Ren::DivCeil<uint32_t>(view_state_.out_res[1], DebugVelocity::GRP_SIZE_Y), 1u};

        DebugVelocity::Params uniform_params;
        uniform_params.img_size = Ren::Vec2u{view_state_.out_res};

        DispatchCompute(fg.cmd_buf(), pi_debug_velocity_, fg.storages(), grp_count, bindings, &uniform_params,
                        sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
    });

    return output;
}

Eng::FgImgRWHandle Eng::Renderer::AddDebugGBufferPass(const FrameTextures &frame_textures, const int pi_index) {
    auto &debug_gbuffer = fg_builder_.AddNode("DEBUG GBUFFER");

    struct PassData {
        FgImgROHandle in_depth;
        FgImgROHandle in_albedo;
        FgImgROHandle in_normals;
        FgImgROHandle in_specular;
        FgImgRWHandle out_color;
    };

    auto *data = fg_builder_.AllocTempData<PassData>();
    data->in_depth = debug_gbuffer.AddTextureInput(frame_textures.depth, Ren::eStage::ComputeShader);
    data->in_albedo = debug_gbuffer.AddTextureInput(frame_textures.albedo, Ren::eStage::ComputeShader);
    data->in_normals = debug_gbuffer.AddTextureInput(frame_textures.normal, Ren::eStage::ComputeShader);
    data->in_specular = debug_gbuffer.AddTextureInput(frame_textures.specular, Ren::eStage::ComputeShader);

    FgImgRWHandle output;
    { // Output texture
        FgImgDesc desc;
        desc.w = view_state_.out_res[0];
        desc.h = view_state_.out_res[1];
        desc.format = Ren::eFormat::RGBA8;
        desc.sampling.wrap = Ren::eWrap::ClampToEdge;

        output = data->out_color =
            debug_gbuffer.AddStorageImageOutput("GBuffer Debug", desc, Ren::eStage::ComputeShader);
    }

    debug_gbuffer.set_execute_cb([this, data, pi_index](const FgContext &fg) {
        const Ren::ImageROHandle depth = fg.AccessROImage(data->in_depth);
        const Ren::ImageROHandle albedo = fg.AccessROImage(data->in_albedo);
        const Ren::ImageROHandle normals = fg.AccessROImage(data->in_normals);
        const Ren::ImageROHandle specular = fg.AccessROImage(data->in_specular);

        const Ren::ImageRWHandle output = fg.AccessRWImage(data->out_color);

        const Ren::Binding bindings[] = {{Ren::eBindTarget::TexSampled, DebugGBuffer::DEPTH_TEX_SLOT, {depth, 1}},
                                         {Ren::eBindTarget::TexSampled, DebugGBuffer::ALBEDO_TEX_SLOT, albedo},
                                         {Ren::eBindTarget::TexSampled, DebugGBuffer::NORM_TEX_SLOT, normals},
                                         {Ren::eBindTarget::TexSampled, DebugGBuffer::SPEC_TEX_SLOT, specular},
                                         {Ren::eBindTarget::ImageRW, DebugGBuffer::OUT_IMG_SLOT, output}};

        const auto grp_count = Ren::Vec3u{Ren::DivCeil<uint32_t>(view_state_.out_res[0], DebugGBuffer::GRP_SIZE_X),
                                          Ren::DivCeil<uint32_t>(view_state_.out_res[1], DebugGBuffer::GRP_SIZE_Y), 1u};

        DebugGBuffer::Params uniform_params;
        uniform_params.img_size[0] = view_state_.out_res[0];
        uniform_params.img_size[1] = view_state_.out_res[1];

        DispatchCompute(fg.cmd_buf(), pi_debug_gbuffer_[pi_index], fg.storages(), grp_count, bindings, &uniform_params,
                        sizeof(uniform_params), ctx_.default_descr_alloc(), ctx_.log());
    });

    return output;
}
