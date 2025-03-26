#version 430 core

#include "_cs_common.glsl"
#include "_fs_common.glsl"
#include "fog_common.glsl"
#include "pmj_common.glsl"
#include "gi_cache_common.glsl"

#include "fog_interface.h"

#pragma multi_compile _ GI_CACHE

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = SHADOW_DEPTH_TEX_SLOT) uniform sampler2DShadow g_shadow_depth_tex;
layout(binding = SHADOW_COLOR_TEX_SLOT) uniform sampler2D g_shadow_color_tex;

layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;
layout(binding = FROXELS_TEX_SLOT) uniform sampler3D g_froxels_history_tex;

#ifdef GI_CACHE
    layout(binding = IRRADIANCE_TEX_SLOT) uniform sampler2DArray g_irradiance_tex;
    layout(binding = DISTANCE_TEX_SLOT) uniform sampler2DArray g_distance_tex;
    layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;
#endif

layout(binding = OUT_FROXELS_IMG_SLOT, rgba16f) uniform writeonly image3D g_out_froxels_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec3 icoord = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(icoord, g_params.froxel_res.xyz))) {
        return;
    }

    const uint px_hash = hash((gl_GlobalInvocationID.x << 16) | gl_GlobalInvocationID.y);
    const float offset_rand = get_scrambled_2d_rand(g_random_seq, RAND_DIM_FOG_LIGHT_PICK, px_hash, int(g_params.frame_index)).y;

    const vec3 pos_uvw = froxel_to_uvw(icoord, offset_rand, g_params.froxel_res.xyz);
    const vec4 pos_cs = vec4(2.0 * pos_uvw.xy - 1.0, pos_uvw.z 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec3 view_ray_ws = normalize(pos_ws - g_shrd_data.cam_pos_and_exp.xyz);
    const float view_dist = length(pos_ws - g_shrd_data.cam_pos_and_exp.xyz);

    const float k = saturate(abs(pos_ws.y) / 20.0);

    float density = mix(g_params.density, 0.0, k);
    vec3 light_total = vec3(0.0);

    if (dot(g_shrd_data.sun_col_point.xyz, g_shrd_data.sun_col_point.xyz) > 0.0 && g_shrd_data.sun_dir.y > 0.0) {
        vec3 shadow_uvs = (g_shrd_data.shadowmap_regions[3].clip_from_world * vec4(pos_ws, 1.0)).xyz;
        shadow_uvs.xy = 0.5 * shadow_uvs.xy + 0.5;

        vec3 sun_visibility = vec3(1.0);

        // NOTE: We have to check the bounds manually as we access outside of scene bounds
        if (all(lessThan(shadow_uvs.xy, vec2(0.999))) && all(greaterThan(shadow_uvs.xy, vec2(0.001)))) {
            shadow_uvs.xy *= vec2(0.25, 0.5);
            shadow_uvs.xy += vec2(0.25, 0.5);
#if defined(VULKAN)
            shadow_uvs.y = 1.0 - shadow_uvs.y;
#endif // VULKAN

            sun_visibility = SampleShadowPCF5x5(g_shadow_depth_tex, g_shadow_color_tex, shadow_uvs);
        }
        light_total += sun_visibility * g_shrd_data.sun_col_point.xyz * HenyeyGreenstein(dot(view_ray_ws, g_shrd_data.sun_dir.xyz), g_params.anisotropy);
    }

#ifdef GI_CACHE
    for (int i = 0; i < PROBE_VOLUMES_COUNT; ++i) {
        const float weight = get_volume_blend_weight(pos_ws, g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz);
        if (weight > 0.0 || i == PROBE_VOLUMES_COUNT - 1) {
            const vec3 avg_radiance = get_volume_irradiance(i, g_irradiance_tex, g_distance_tex, g_offset_tex, pos_ws, vec3(0.0), view_ray_ws,
                                                            g_shrd_data.probe_volumes[i].scroll.xyz, g_shrd_data.probe_volumes[i].origin.xyz, g_shrd_data.probe_volumes[i].spacing.xyz, false, false);
            light_total += (1.0 / M_PI) * avg_radiance;

            break;
        }
    }
#endif

    light_total *= density;
    light_total = compress_hdr(light_total, g_shrd_data.cam_pos_and_exp.w);

    { // history accumulation
        const vec3 pos_uvw_no_offset = froxel_to_uvw(icoord, 0.5, g_params.froxel_res.xyz);
        const vec4 pos_cs_no_offset = vec4(2.0 * pos_uvw_no_offset.xy - 1.0, pos_uvw_no_offset.z 1.0);
        const vec3 pos_ws_no_offset = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs_no_offset);

        const vec3 hist_pos_cs = TransformToClipSpace(g_shrd_data.prev_clip_from_world, pos_ws_no_offset);
        const float hist_lin_depth = LinearizeDepth(hist_pos_cs.z, g_shrd_data.clip_info);
        const vec3 hist_uvw = cs_to_uvw(hist_pos_cs, hist_lin_depth);

        if (all(greaterThanEqual(hist_uvw, vec3(0.0))) && all(lessThanEqual(hist_uvw, vec3(1.0)))) {
            vec4 hist_fetch = textureLod(g_froxels_history_tex, hist_uvw, 0.0);
            hist_fetch.xyz *= g_params.hist_weight;

            const float HistoryWeightMin = 0.75;
            const float HistoryWeightMax = 0.95;

            const float lum_curr = lum(light_total);
            const float lum_hist = lum(hist_fetch.xyz);

            const float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.001);
            const float unbiased_weight = 1.0 - unbiased_diff;
            const float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
            const float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

            light_total = mix(light_total, hist_fetch.xyz, history_weight);
            density = mix(density, hist_fetch.w, history_weight);
        }
    }

    imageStore(g_out_froxels_img, icoord, vec4(light_total, density));
}
