#version 430 core

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"

#include "probe_classify_interface.h"

#pragma multi_compile _ RESET

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = RAY_DATA_TEX_SLOT) uniform sampler2DArray g_ray_data;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2DArray g_out_img;

vec3 _get_probe_pos_ws(const ivec3 coords, const ivec3 offset, const vec3 grid_origin, const vec3 grid_spacing) {
    const vec3 pos_ws = get_probe_pos_ws(coords, offset, grid_origin, grid_spacing);

    const int probe_index = get_scrolling_probe_index(coords, offset);
    const ivec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);

    return pos_ws + imageLoad(g_out_img, tex_coords).xyz;
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const int probe_index = int(gl_GlobalInvocationID.x);
    if (probe_index >= PROBE_VOLUME_RES * PROBE_VOLUME_RES * PROBE_VOLUME_RES) {
        return;
    }

    const ivec3 output_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    vec4 offset = imageLoad(g_out_img, output_coords);

#ifdef RESET
    offset.w = PROBE_STATE_ACTIVE;
#else
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

    if (backface_count > (PROBE_FIXED_RAYS_COUNT / 4)) {
        offset.w = PROBE_STATE_INACTIVE;
    } else {
        const ivec3 probe_coords = get_probe_coords(probe_index);
        const vec3 probe_pos = _get_probe_pos_ws(probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz);

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
                offset.w = (outdoor_count > (PROBE_FIXED_RAYS_COUNT - backface_count) / 2) ? PROBE_STATE_ACTIVE_OUTDOOR : PROBE_STATE_ACTIVE;
                break;
            }
        }
    }
#endif

    imageStore(g_out_img, output_coords, offset);
}