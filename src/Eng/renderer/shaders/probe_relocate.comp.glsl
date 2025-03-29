#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"

#include "probe_relocate_interface.h"

#pragma multi_compile _ PARTIAL
#pragma multi_compile _ RESET

#if defined(RESET) && defined(PARTIAL)
    #pragma dont_compile
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = RAY_DATA_TEX_SLOT) uniform sampler2DArray g_ray_data;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2DArray g_out_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    int probe_index = int(gl_GlobalInvocationID.x);
    if (probe_index >= PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Y * PROBE_VOLUME_RES_Z) {
        return;
    }

    const ivec3 probe_coords = get_probe_coords(probe_index);
    probe_index = get_scrolling_probe_index(probe_coords, g_params.grid_scroll.xyz);
    const ivec3 output_coords = get_probe_texel_coords(probe_index, g_params.volume_index);

    vec4 offset = imageLoad(g_out_img, output_coords);

#ifdef RESET
    offset.xyz = vec3(0.0);
#else // RESET

#ifdef PARTIAL
    const bool is_scrolling_plane_probe = IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz);
    const ivec3 oct_index = get_probe_coords(probe_index) & 1;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1) | (oct_index.z << 2)) != g_params.oct_index;
    if (!is_scrolling_plane_probe && is_wrong_oct) {
        return;
    }
#endif // PARTIAL

    int closest_back_index = -1, closest_front_index = -1, farthest_front_index = -1;
    float closest_back_dist = 1e27, closest_front_dist = 1e27, farthest_front_dist = 0.0;
    float backface_count = 0.0;

    for (int i = 0; i < PROBE_FIXED_RAYS_COUNT; ++i) {
        const ivec3 ray_data_coords = get_ray_data_coords(i, probe_index);

        float hit_dist = texelFetch(g_ray_data, ray_data_coords, 0).w;
        if (hit_dist < 0.0) {
            backface_count += 1.0;
            hit_dist = -5.0 * hit_dist;
            if (hit_dist < closest_back_dist) {
                closest_back_dist = hit_dist;
                closest_back_index = i;
            }
        } else {
            if (hit_dist < closest_front_dist) {
                closest_front_dist = hit_dist;
                closest_front_index = i;
            } else if (hit_dist > farthest_front_dist) {
                farthest_front_dist = hit_dist;
                farthest_front_index = i;
            }
        }
    }

    const float MinFrontDistance = 1.0 * length(g_params.grid_spacing.xyz);
    vec3 full_offset = vec3(1e27);

    if (closest_back_index != -1 && (backface_count / float(PROBE_FIXED_RAYS_COUNT)) > 0.25) {
        const vec3 closest_back_dir = get_probe_ray_dir(closest_back_index, g_params.quat_rot);
        full_offset = offset.xyz + closest_back_dir * (closest_back_dist + MinFrontDistance * 0.5);
    } else if (closest_front_dist < MinFrontDistance) {
        const vec3 closest_front_dir = get_probe_ray_dir(closest_front_index, g_params.quat_rot);
        vec3 farthest_front_dir = get_probe_ray_dir(farthest_front_index, g_params.quat_rot);
        if (dot(closest_front_dir, farthest_front_dir) <= 0.0) {
            farthest_front_dir *= min(farthest_front_dist, 1.0);
            full_offset = offset.xyz + farthest_front_dir;
        }
    } else if (closest_front_dist > MinFrontDistance) {
        const float move_back_margin = min(closest_front_dist - MinFrontDistance, length(offset.xyz));
        const vec3 move_back_dir = normalize(-offset.xyz);
        full_offset = offset.xyz + move_back_margin * move_back_dir;
    }

    const vec3 normalized_offset = full_offset / g_params.grid_spacing.xyz;
    if (dot(normalized_offset, normalized_offset) < 0.45 * 0.45) {
        offset.xyz = full_offset;
    }
#endif // RESET
    imageStore(g_out_img, output_coords, offset);
}