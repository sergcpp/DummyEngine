#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"

#include "probe_blend_interface.h"

#pragma multi_compile RADIANCE DISTANCE

#if defined(RADIANCE)
    const int TEXEL_RES = PROBE_IRRADIANCE_RES;
#elif defined(DISTANCE)
    const int TEXEL_RES = PROBE_DISTANCE_RES;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = RAY_DATA_TEX_SLOT) uniform sampler2DArray g_ray_data;
layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

#if defined(RADIANCE)
    layout(binding = OUT_IMG_SLOT, rgba16f) uniform coherent image2DArray g_out_img;
#elif defined(DISTANCE)
    layout(binding = OUT_IMG_SLOT, rg16f) uniform coherent image2DArray g_out_img;
#endif

layout (local_size_x = TEXEL_RES, local_size_y = TEXEL_RES, local_size_z = 1) in;

void main() {
    const int probe_index = get_probe_index(ivec3(gl_GlobalInvocationID), TEXEL_RES);

    const bool is_border_texel = (gl_LocalInvocationID.x == 0) || (gl_LocalInvocationID.x == (TEXEL_RES - 2 + 1)) ||
                                 (gl_LocalInvocationID.y == 0) || (gl_LocalInvocationID.y == (TEXEL_RES - 2 + 1));

    if (!is_border_texel) {
        const bool is_scrolling_plane_probe = IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz);

        const ivec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
        if (!is_scrolling_plane_probe && texelFetch(g_offset_tex, tex_coords, 0).w < 0.5) {
            return;
        }

        const ivec3 thread_coords = ivec3(gl_WorkGroupID.x * (TEXEL_RES - 2),
                                          gl_WorkGroupID.y * (TEXEL_RES - 2),
                                          gl_GlobalInvocationID.z) + ivec3(gl_LocalInvocationID) - ivec3(1, 1, 0);

        const vec2 probe_oct_uv = get_normalized_oct_coords(thread_coords.xy, TEXEL_RES);
        const vec3 probe_ray_dir = get_oct_dir(probe_oct_uv);

        vec4 result = vec4(0.0);
        int backfaces = 0;

        for (int i = PROBE_FIXED_RAYS_COUNT; i < PROBE_TOTAL_RAYS_COUNT; ++i) {
            const vec3 ray_dir = get_probe_ray_dir(i, g_params.quat_rot);

            float weight = clamp(dot(probe_ray_dir, ray_dir), 0.0, 1.0);

            const ivec3 ray_data_coords = get_ray_data_coords(i, probe_index);

            vec4 ray_data = texelFetch(g_ray_data, ray_data_coords + ivec3(0, 0, g_params.input_offset), 0);

#if defined(RADIANCE)
            ray_data.xyz = decompress_hdr(ray_data.xyz);

            if (ray_data.a < 0.0) {
                ++backfaces;
                if (!is_scrolling_plane_probe && backfaces > 24) {
                    return;
                }
            }

            result += vec4(weight * ray_data.rgb, weight);
#elif defined(DISTANCE)
            const float max_ray_distance = length(g_params.grid_spacing) * 1.5;
            float ray_distance = min(abs(ray_data.a), max_ray_distance);
            if (ray_data.a < 0.0) {
                // add thickness to backfacing surfaces
                //ray_distance = max(0.0, ray_distance - 0.25 * max_ray_distance);
            }

            weight = pow(weight, 50.0);
            result += vec4(weight * ray_distance, weight * ray_distance * ray_distance, 0.0, weight);
#endif
        }

        float epsilon = float(PROBE_TOTAL_RAYS_COUNT - PROBE_FIXED_RAYS_COUNT);
        epsilon *= 1e-9;

        result.rgb *= 1.0 / (2.0 * max(result.a, epsilon));

        const vec4 probe_irradiance_mean = imageLoad(g_out_img, ivec3(gl_GlobalInvocationID.xy, g_params.output_offset + gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES));

#if defined(RADIANCE)
        // Stable 2-sample accumulation (approximate)
        float lum_curr = lum(result.rgb);
        float lum_prev = probe_irradiance_mean.w;
        float lum_hist = lum(probe_irradiance_mean.rgb);
        float lum_desired = 0.5 * (lum_curr + lum_prev);

        float history_weight = clamp((lum_desired - lum_curr) / (lum_hist - lum_curr), 0.0, 0.97);
        //float history_weight = g_params.hysteresis;
#elif defined(DISTANCE)
        float history_weight = g_params.hysteresis;
#endif
        if (is_scrolling_plane_probe || dot(probe_irradiance_mean.rgb, probe_irradiance_mean.rgb) == 0.0) {
            history_weight = 0.0;
        }

#if defined(RADIANCE)
        result = vec4(mix(result.rgb, probe_irradiance_mean.rgb, history_weight), lum_curr);
#elif defined(DISTANCE)
        result = vec4(mix(result.rg, probe_irradiance_mean.rg, history_weight), 0.0, 1.0);
#endif

        imageStore(g_out_img, ivec3(gl_GlobalInvocationID.xy, g_params.output_offset + gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES), result);
    }

    groupMemoryBarrier(); barrier();

    if (is_border_texel) {
        const bool is_corner_texel = (gl_LocalInvocationID.x == 0 || gl_LocalInvocationID.x == (TEXEL_RES - 1)) && (gl_LocalInvocationID.y == 0 || gl_LocalInvocationID.y == (TEXEL_RES - 1));
        const bool is_row_texel = (gl_LocalInvocationID.x > 0 && gl_LocalInvocationID.x < (TEXEL_RES - 1));

        ivec3 copy_coords = ivec3(gl_WorkGroupID.x * TEXEL_RES, gl_WorkGroupID.y * TEXEL_RES, gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES);

        if (is_corner_texel) {
            copy_coords.x += int(gl_LocalInvocationID.x > 0 ? 1 : (TEXEL_RES - 2));
            copy_coords.y += int(gl_LocalInvocationID.y > 0 ? 1 : (TEXEL_RES - 2));
        } else if(is_row_texel) {
            copy_coords.x += int((TEXEL_RES - 1) - gl_LocalInvocationID.x);
            copy_coords.y += int(gl_LocalInvocationID.y + ((gl_LocalInvocationID.y > 0) ? -1 : 1));
        } else {
            copy_coords.x += int(gl_LocalInvocationID.x + ((gl_LocalInvocationID.x > 0) ? -1 : 1));
            copy_coords.y += int((TEXEL_RES - 1) - gl_LocalInvocationID.y);
        }

        const vec4 result = imageLoad(g_out_img, copy_coords);
        imageStore(g_out_img, ivec3(gl_GlobalInvocationID.xy, g_params.output_offset + gl_GlobalInvocationID.z + g_params.volume_index * PROBE_VOLUME_RES), result);
    }
}
