#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"

#include "probe_blend_interface.h"

#pragma multi_compile IRRADIANCE DISTANCE
#pragma multi_compile _ STOCH_LIGHTS
#pragma multi_compile _ PARTIAL

#if defined(DISTANCE) && defined(STOCH_LIGHTS)
    #pragma dont_compile
#endif

#if defined(IRRADIANCE)
    const uint TEXEL_RES = PROBE_IRRADIANCE_RES;
#elif defined(DISTANCE)
    const uint TEXEL_RES = PROBE_DISTANCE_RES;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = RAY_DATA_TEX_SLOT) uniform sampler2DArray g_ray_data;
layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform coherent image2DArray g_out_img;

layout (local_size_x = TEXEL_RES, local_size_y = TEXEL_RES, local_size_z = 1) in;

void main() {
    const uint probe_index = get_probe_index(gl_GlobalInvocationID, TEXEL_RES);
    const bool is_scrolling_plane_probe = IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz);

    const uvec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    const bool is_inactive = texelFetch(g_offset_tex, ivec3(tex_coords), 0).w < 0.5;

#ifdef PARTIAL
    const uvec3 oct_index = get_probe_coords(probe_index) & 1u;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1u) | (oct_index.z << 2u)) != g_params.oct_index;
#else
    const bool is_wrong_oct = false;
#endif

    if (!is_scrolling_plane_probe && (is_inactive || is_wrong_oct)) {
        return;
    }

    const uvec3 output_coords = uvec3(gl_GlobalInvocationID.xy, gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES_Y);

    const bool is_border_texel = (gl_LocalInvocationID.x == 0) || (gl_LocalInvocationID.x == (TEXEL_RES - 2 + 1)) ||
                                 (gl_LocalInvocationID.y == 0) || (gl_LocalInvocationID.y == (TEXEL_RES - 2 + 1));
    if (!is_border_texel) {
        const uvec3 thread_coords = uvec3(gl_WorkGroupID.xy * (TEXEL_RES - 2),
                                          gl_GlobalInvocationID.z) + gl_LocalInvocationID - uvec3(1, 1, 0);

        const vec2 probe_oct_uv = get_normalized_oct_coords(thread_coords.xy, TEXEL_RES);
        const vec3 probe_ray_dir = get_oct_dir(probe_oct_uv);

        vec4 result = vec4(0.0);
        float total_weight = 0.0;
        int backfaces = 0;

        for (uint i = PROBE_FIXED_RAYS_COUNT; i < PROBE_TOTAL_RAYS_COUNT; ++i) {
            const vec3 ray_dir = get_probe_ray_dir(i, g_params.quat_rot);

            float weight = saturate(dot(probe_ray_dir, ray_dir));

            const uvec3 ray_data_coords = get_ray_data_coords(i, probe_index);

            vec4 ray_data = texelFetch(g_ray_data, ivec3(ray_data_coords), 0);

#if defined(IRRADIANCE)
            ray_data.xyz = (ray_data.xyz / g_params.pre_exposure);

            if (ray_data.a < 0.0) {
                ++backfaces;
                if (!is_scrolling_plane_probe && backfaces > 24) {
                    return;
                }
            }

            result.xyz += weight * ray_data.xyz;
            total_weight += weight;

#elif defined(DISTANCE)
            const float max_ray_distance = length(g_params.grid_spacing) * 1.5;
            float ray_distance = min(abs(ray_data.a), max_ray_distance);
            if (ray_data.a < 0.0) {
                // add thickness to backfacing surfaces
                //ray_distance = max(0.0, ray_distance - 0.25 * max_ray_distance);
            }
            const float ray_distance_sqr = sqr(ray_distance);

            weight = pow(weight, 50.0);
            result += weight * vec4(ray_distance, ray_distance_sqr, ray_distance * ray_distance_sqr, sqr(ray_distance_sqr));
            total_weight += weight;
#endif
        }

        float epsilon = float(PROBE_TOTAL_RAYS_COUNT - PROBE_FIXED_RAYS_COUNT);
        epsilon *= 1e-9;

        result *= rcp(max(total_weight, epsilon));

        vec4 direct_light = vec4(0.0);
#if defined(STOCH_LIGHTS)
        for (uint i = 0; i < PROBE_TOTAL_RAYS_COUNT; ++i) {
            const uvec3 ray_data_coords = get_ray_data_coords(i, probe_index);

            const vec3 light_color = (texelFetch(g_ray_data, ivec3(ray_data_coords + uvec3(0, 0, 1 * PROBE_VOLUME_RES_Y)), 0).xyz / g_params.pre_exposure);
            const vec3 light_dir = texelFetch(g_ray_data, ivec3(ray_data_coords + uvec3(0, 0, 2 * PROBE_VOLUME_RES_Y)), 0).xyz;

            const float weight = saturate(dot(probe_ray_dir, light_dir));

            direct_light += vec4(weight * light_color, 1.0);
        }
        result.xyz += (direct_light.xyz * rcp(direct_light.w));
#endif

        const vec4 probe_mean = imageLoad(g_out_img, ivec3(output_coords));

#if defined(IRRADIANCE)
        result.xyz = pow(result.xyz, vec3(1.0 / PROBE_RADIANCE_EXP));

        // Stable 2-sample accumulation (approximate)
        const float lum_curr = lum(result.xyz);
        const float lum_prev = probe_mean.w;
        const float lum_hist = lum(probe_mean.xyz);
        const float lum_desired = 0.5 * (lum_curr + lum_prev);

        const float diff = lum_hist - lum_curr;
        float history_weight = abs(diff) > FLT_EPS ? clamp((lum_desired - lum_curr) / diff, 0.0, 0.92) : 0.92;
#elif defined(DISTANCE)
        float history_weight = 0.92;
#endif
        if (is_scrolling_plane_probe || max_component(probe_mean.xyz) == 0.0) {
            history_weight = 0.0;
        }

#if defined(IRRADIANCE)
        result = vec4(mix(result.xyz, probe_mean.xyz, history_weight), lum_curr);
#elif defined(DISTANCE)
        result = mix(result, probe_mean, history_weight);
#endif

        imageStore(g_out_img, ivec3(output_coords), result);
    }

    groupMemoryBarrier(); barrier();

    if (is_border_texel) {
        const bool is_corner_texel = (gl_LocalInvocationID.x == 0 || gl_LocalInvocationID.x == (TEXEL_RES - 1)) && (gl_LocalInvocationID.y == 0 || gl_LocalInvocationID.y == (TEXEL_RES - 1));
        const bool is_row_texel = (gl_LocalInvocationID.x > 0 && gl_LocalInvocationID.x < (TEXEL_RES - 1));

        ivec3 copy_coords = ivec3(gl_WorkGroupID.x * TEXEL_RES, gl_WorkGroupID.y * TEXEL_RES, gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES_Y);

        if (is_corner_texel) {
            copy_coords.x += int(gl_LocalInvocationID.x > 0 ? 1 : (TEXEL_RES - 2));
            copy_coords.y += int(gl_LocalInvocationID.y > 0 ? 1 : (TEXEL_RES - 2));
        } else if (is_row_texel) {
            copy_coords.x += int((TEXEL_RES - 1) - gl_LocalInvocationID.x);
            copy_coords.y += int(gl_LocalInvocationID.y + ((gl_LocalInvocationID.y > 0) ? -1 : 1));
        } else {
            copy_coords.x += int(gl_LocalInvocationID.x + ((gl_LocalInvocationID.x > 0) ? -1 : 1));
            copy_coords.y += int((TEXEL_RES - 1) - gl_LocalInvocationID.y);
        }

        const vec4 result = imageLoad(g_out_img, copy_coords);
        imageStore(g_out_img, ivec3(output_coords), result);
    }
}
