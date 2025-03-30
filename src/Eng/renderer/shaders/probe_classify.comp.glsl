#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"

#include "probe_classify_interface.h"

#pragma multi_compile _ VOL RESET
#pragma multi_compile _ PARTIAL

#if defined(RESET) && defined(PARTIAL)
    #pragma dont_compile
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

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
    offset.w = PROBE_STATE_ACTIVE;
#else // RESET

#ifdef PARTIAL
    const bool is_scrolling_plane_probe = IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz);
    const ivec3 oct_index = get_probe_coords(probe_index) & 1;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1) | (oct_index.z << 2)) != g_params.oct_index;
    if (!is_scrolling_plane_probe && is_wrong_oct) {
        return;
    }
#endif // PARTIAL

    int backface_count = 0, outdoor_count = 0;
    float hit_distances[PROBE_FIXED_RAYS_COUNT];

    for (int i = 0; i < PROBE_FIXED_RAYS_COUNT; ++i) {
        const ivec3 ray_data_coords = get_ray_data_coords(i, probe_index);

        hit_distances[i] = texelFetch(g_ray_data, ray_data_coords, 0).w;
        if (hit_distances[i] < 0.0) {
            ++backface_count;
        }
        if (hit_distances[i] > 100.0) {
            ++outdoor_count;
        }
    }

    const vec3 probe_pos = get_probe_pos_ws(probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz) + offset.xyz;

    if (backface_count > (PROBE_FIXED_RAYS_COUNT / 4)) {
        offset.w = PROBE_STATE_INACTIVE;
    } else {
        offset.w = PROBE_STATE_INACTIVE;
        for (int i = 0; i < PROBE_FIXED_RAYS_COUNT; ++i) {
            if (hit_distances[i] < 0.0) {
                continue;
            }

            const vec3 probe_ray_dir = get_probe_ray_dir(i, g_params.quat_rot);

            const vec3 x_normal = vec3(probe_ray_dir.x / max(abs(probe_ray_dir.x), 0.000001), 0.0, 0.0);
            const vec3 y_normal = vec3(0.0, probe_ray_dir.y / max(abs(probe_ray_dir.y), 0.000001), 0.0);
            const vec3 z_normal = vec3(0.0, 0.0, probe_ray_dir.z / max(abs(probe_ray_dir.z), 0.000001));

            const vec3 p0x = probe_pos + g_params.grid_spacing.x * x_normal;
            const vec3 p0y = probe_pos + g_params.grid_spacing.y * y_normal;
            const vec3 p0z = probe_pos + g_params.grid_spacing.z * z_normal;

            vec3 distances = vec3(
                dot(p0x - probe_pos, x_normal) / max(dot(probe_ray_dir, x_normal), 0.000001),
                dot(p0y - probe_pos, y_normal) / max(dot(probe_ray_dir, y_normal), 0.000001),
                dot(p0z - probe_pos, z_normal) / max(dot(probe_ray_dir, z_normal), 0.000001)
            );

            if (distances.x == 0.0) distances.x = 1e27;
            if (distances.y == 0.0) distances.y = 1e27;
            if (distances.z == 0.0) distances.z = 1e27;

            const float max_distance = min(distances.x, min(distances.y, distances.z));

            if (hit_distances[i] <= max_distance) {
                offset.w = (outdoor_count > (PROBE_FIXED_RAYS_COUNT - backface_count) / 3) ? PROBE_STATE_ACTIVE_OUTDOOR : PROBE_STATE_ACTIVE;
                break;
            }
        }
    }

#ifdef VOL
    if (offset.w < 0.5) {
        const bool is_inside_volume = bbox_test(probe_pos, g_params.vol_bbox_min.xyz - g_params.grid_spacing.xyz,
                                                           g_params.vol_bbox_max.xyz + g_params.grid_spacing.xyz);
        if (is_inside_volume) {
            bool is_inside_frustum = true;
            for (int i = 0; i < 6; ++i) {
                const float dist = dot(probe_pos, g_shrd_data.frustum_planes[i].xyz) + g_shrd_data.frustum_planes[i].w;
                is_inside_frustum = is_inside_frustum && (dist > -g_params.grid_spacing.w);
            }
            if (is_inside_frustum) {
                offset.w = (outdoor_count > (PROBE_FIXED_RAYS_COUNT - backface_count) / 3) ? PROBE_STATE_ACTIVE_OUTDOOR : PROBE_STATE_ACTIVE;
            }
        }
    }
#endif


#endif // RESET

    imageStore(g_out_img, output_coords, offset);
}