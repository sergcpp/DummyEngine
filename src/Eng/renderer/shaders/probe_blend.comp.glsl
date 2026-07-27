#version 430 core

#include "_cs_common.glsl"
#include "principled_common.glsl"
#include "gi_cache_common.glsl"
#include "rad_cache_common.glsl"

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

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(std430, binding = RAY_HITS_BUF_SLOT) readonly buffer RayHitsList {
    uint g_ray_hits[];
};

layout(std430, binding = CACHE_ENTRIES_BUF_SLOT) readonly buffer CacheEntries {
    uint g_cache_entries[];
};

layout(std430, binding = CACHE_VOXELS_BUF_SLOT) readonly buffer CacheVoxels {
    uint g_cache_voxels[];
};

#if defined(IRRADIANCE)
layout(std430, binding = LIGHTS_BUF_SLOT) readonly buffer LightsData {
    _light_item_t g_lights[];
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube g_env_tex;
#endif

layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

#if defined(STOCH_LIGHTS)
    layout(std430, binding = DIRECT_LIGHT_BUF_SLOT) readonly buffer DirectLightData {
        uvec2 g_sh1_direct_light[];
    };
#endif

layout(binding = OUT_IMG_SLOT, rgba16f) uniform coherent image2DArray g_out_img;

bool hash_map_find(const uint hash_key, inout uint cache_entry, out uint bucket_offset) {
    const uint base_slot = hash_map_base_slot(hash_key);
    for (bucket_offset = 0; bucket_offset < HASH_GRID_HASH_MAP_BUCKET_SIZE; ++bucket_offset) {
        const uint stored_hash_key = g_cache_entries[base_slot + bucket_offset];
        if (stored_hash_key == hash_key) {
            cache_entry = base_slot + bucket_offset;
            return true;
        }
    }
    return false;
}

uint find_entry(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uint hash_key = compute_hash(p, backfacing, cam_pos);
    uint cache_entry = HASH_GRID_INVALID_CACHE_ENTRY, collisions_count;
    hash_map_find(hash_key, cache_entry, collisions_count);
    return cache_entry;
}

layout (local_size_x = TEXEL_RES, local_size_y = TEXEL_RES, local_size_z = 1) in;

void main() {
    uint probe_index = get_probe_index(gl_GlobalInvocationID, TEXEL_RES);
    const bool is_scrolling_plane_probe = IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz);

    const uvec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    const vec4 offset = texelFetch(g_offset_tex, ivec3(tex_coords), 0);
    const bool is_inactive = offset.w < 0.5;

    const uvec3 probe_coords = get_probe_coords(probe_index);
#ifdef PARTIAL
    const uvec3 oct_index = probe_coords & 1u;
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

        const uvec3 noscroll_probe_coords = uvec3((ivec3(probe_coords) - g_params.grid_scroll.xyz +
                                                   ivec3(PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Y, PROBE_VOLUME_RES_Z)) %
                                                   ivec3(PROBE_VOLUME_RES_X, PROBE_VOLUME_RES_Y, PROBE_VOLUME_RES_Z));
        const vec3 probe_pos = get_probe_pos_ws(noscroll_probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz) + offset.xyz;

        const vec2 probe_oct_uv = get_normalized_oct_coords(thread_coords.xy, TEXEL_RES);
        const vec3 probe_ray_dir = get_oct_dir(probe_oct_uv);

        vec4 result = vec4(0.0);
        float total_weight = 0.0;
        int backfaces = 0;

        uint read_offset = (PROBE_TOTAL_RAYS_COUNT * probe_index + PROBE_FIXED_RAYS_COUNT) * RAY_HITS_STRIDE;
        for (uint ray_index = PROBE_FIXED_RAYS_COUNT; ray_index < PROBE_TOTAL_RAYS_COUNT; ++ray_index) {
            const vec3 ray_dir = get_probe_ray_dir(ray_index, g_params.quat_rot);

            const uint hit_data0 = g_ray_hits[read_offset + 0];
            const uint hit_data2 = g_ray_hits[read_offset + 2];

            const uvec3 ray_data_coords = get_ray_data_coords(ray_index, probe_index);

            float weight = saturate(dot(probe_ray_dir, ray_dir));

            vec4 ray_data = vec4(0.0, 0.0, 0.0, 1e27);
            if (hit_data2 == 0xffffffffu) {
#if defined(IRRADIANCE)
                vec3 throughput = UnpackRGB565(hit_data0);

                // Check portal lights intersection
                for (int i = 0; i < MAX_PORTALS_TOTAL && g_shrd_data.portals[i / 4][i % 4] != 0xffffffff; ++i) {
                    const _light_item_t litem = g_lights[g_shrd_data.portals[i / 4][i % 4]];

                    const vec3 light_pos = litem.pos_and_radius.xyz;
                    vec3 light_u = litem.u_and_reg.xyz, light_v = litem.v_and_blend.xyz;
                    const vec3 light_forward = normalize(cross(light_u, light_v));

                    const float plane_dist = dot(light_forward, light_pos);
                    const float cos_theta = dot(ray_dir, light_forward);
                    const float t = (plane_dist - dot(light_forward, probe_pos)) / min(cos_theta, -FLT_EPS);

                    if (cos_theta < 0.0 && t > 0.0) {
                        light_u /= dot(light_u, light_u);
                        light_v /= dot(light_v, light_v);

                        const vec3 p = probe_pos + ray_dir * t;
                        const vec3 vi = p - light_pos;
                        const float a1 = dot(light_u, vi);
                        if (a1 >= -1.0 && a1 <= 1.0) {
                            const float a2 = dot(light_v, vi);
                            if (a2 >= -1.0 && a2 <= 1.0) {
                                throughput = vec3(0.0);
                                break;
                            }
                        }
                    }
                }

                const vec3 rotated_dir = rotate_xz(ray_dir, g_shrd_data.env_col.w);
                const float env_mip_count = g_shrd_data.ambient_hack.w;

                ray_data.xyz = throughput * g_shrd_data.env_col.xyz * textureLod(g_env_tex, rotated_dir, env_mip_count - 4.0).xyz;
#endif
            } else {
                ray_data.w = uintBitsToFloat(g_ray_hits[read_offset + 1]);
                const bool backfacing = (ray_data.w < 0.0);
                ray_data.w = abs(ray_data.w);

#if defined(IRRADIANCE)
                const vec3 throughput = UnpackRGB565(hit_data0 & 0xffffu);

                const vec3 P = probe_pos + ray_dir * ray_data.w;
                const uint cache_entry = find_entry(P, backfacing, g_shrd_data.cam_pos_and_exp.xyz);
                if (cache_entry != HASH_GRID_INVALID_CACHE_ENTRY) {
                    ray_data.xyz = throughput * abs(vec3(
                        unpackHalf2x16(g_cache_voxels[2 * cache_entry + 0]),
                        unpackHalf2x16(g_cache_voxels[2 * cache_entry + 1]).x)) * RAD_CACHE_RADIANCE_COMPRESSION;
                } else {
                    // Ignore missing data (to not make the result darker)
                    weight = 0.0;
                }
#endif
            }

#if defined(IRRADIANCE)
            if (ray_data.w < 0.0) {
                ++backfaces;
                if (!is_scrolling_plane_probe && backfaces > 24) {
                    return;
                }
            }

            result.xyz += weight * ray_data.xyz;
            total_weight += weight;

#elif defined(DISTANCE)
            const float max_ray_distance = length(g_params.grid_spacing) * 1.5;
            float ray_distance = min(abs(ray_data.w), max_ray_distance);
            const float ray_distance_sqr = sqr(ray_distance);

            weight = pow(weight, 50.0);
            result += weight * vec4(ray_distance, ray_distance_sqr, ray_distance * ray_distance_sqr, sqr(ray_distance_sqr));
            total_weight += weight;
#endif

            read_offset += RAY_HITS_STRIDE;
        }

        float epsilon = float(PROBE_TOTAL_RAYS_COUNT - PROBE_FIXED_RAYS_COUNT);
        epsilon *= 1e-9;

        result *= rcp(max(total_weight, epsilon));

#if defined(STOCH_LIGHTS)
        const vec4 sh1_r = unpackHalf4x16(g_sh1_direct_light[3 * probe_index + 0]);
        const vec4 sh1_g = unpackHalf4x16(g_sh1_direct_light[3 * probe_index + 1]);
        const vec4 sh1_b = unpackHalf4x16(g_sh1_direct_light[3 * probe_index + 2]);

        const vec3 direct_irradiance = vec3(max(dot(sh1_r, vec4(probe_ray_dir, 1.0)), 0.0),
                                            max(dot(sh1_g, vec4(probe_ray_dir, 1.0)), 0.0),
                                            max(dot(sh1_b, vec4(probe_ray_dir, 1.0)), 0.0));
        result.xyz += (direct_irradiance / g_params.pre_exposure);
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
