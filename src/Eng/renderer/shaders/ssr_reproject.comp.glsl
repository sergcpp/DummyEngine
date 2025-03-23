#version 430 core
#extension GL_ARB_shading_language_packing : require

// NOTE: This is not used for now
#if !USE_FP16
    #define float16_t float
    #define f16vec2 vec2
    #define f16vec3 vec3
    #define f16vec4 vec4
#endif

#include "_cs_common.glsl"
#include "rt_common.glsl"
#include "ssr_common.glsl"
#include "ssr_reproject_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = DEPTH_HIST_TEX_SLOT) uniform sampler2D g_depth_hist_tex;
layout(binding = NORM_HIST_TEX_SLOT) uniform usampler2D g_norm_hist_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = REFL_HIST_TEX_SLOT) uniform sampler2D g_refl_hist_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = VARIANCE_HIST_TEX_SLOT) uniform sampler2D g_variance_hist_tex;
layout(binding = SAMPLE_COUNT_HIST_TEX_SLOT) uniform sampler2D g_sample_count_hist_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REPROJECTED_IMG_SLOT, rgba16f) uniform image2D g_out_reprojected_img;
layout(binding = OUT_AVG_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_avg_refl_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;
layout(binding = OUT_SAMPLE_COUNT_IMG_SLOT, r16f) uniform image2D g_out_sample_count_img;

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    f16vec4 refl[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        refl[i] = texelFetch(g_refl_tex, dispatch_thread_id + offset[i], 0);
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(refl[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(refl[i].zw);
    }
}

f16vec4 LoadFromSharedMemoryRaw(const ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
}

// Taken from https://github.com/GPUOpen-Effects/FidelityFX-Denoiser

#define GAUSSIAN_K 3.0

#define LOCAL_NEIGHBORHOOD_RADIUS 4
#define REPROJECTION_NORMAL_SIMILARITY_THRESHOLD 0.8 //0.9999
#define AVG_RADIANCE_LUMINANCE_WEIGHT 0.3
#define REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT 1.5
#define DISOCCLUSION_NORMAL_WEIGHT 1.0 // 1.4
#define DISOCCLUSION_DEPTH_WEIGHT 1.0
#define DISOCCLUSION_THRESHOLD 0.9
#define SAMPLES_FOR_ROUGHNESS(r) (1.0 - exp(-r * 100.0))

float16_t GetLuminanceWeight(const f16vec3 val) {
    const float16_t luma = Luminance(val.xyz);
    const float16_t weight = max(exp(-luma * AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2);
    return weight;
}

float16_t LocalNeighborhoodKernelWeight(const float16_t i) {
    const float16_t radius = LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-GAUSSIAN_K * (i * i) / (radius * radius));
}

struct moments_t {
    f16vec4 mean;
    f16vec4 variance;
};

moments_t EstimateLocalNeighbourhoodInGroup(const ivec2 group_thread_id) {
    moments_t ret;
    ret.mean = vec4(0.0);
    ret.variance = vec4(0.0);

    float16_t accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            const ivec2 index = group_thread_id + ivec2(i, j);
            const f16vec4 radiance = LoadFromSharedMemoryRaw(index);
            const float16_t weight = float16_t(radiance.w > 0.0) * LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
            accumulated_weight += weight;

            ret.mean += radiance * weight;
            ret.variance += radiance * radiance * weight;
        }
    }

    ret.mean /= accumulated_weight;
    ret.variance /= accumulated_weight;

    ret.variance = abs(ret.variance - ret.mean * ret.mean);

    return ret;
}

vec2 GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length) {
    float z = texelFetch(g_depth_tex, dispatch_thread_id, 0).x;
    vec3 ray_vs = vec3(uv, z);

    vec2 unjitter = g_shrd_data.taa_info.xy;

    { // project from screen space to view space
        ray_vs.xy = 2.0 * ray_vs.xy - 1.0;
#if defined(VULKAN)
        ray_vs.y = -ray_vs.y;
#endif
        ray_vs.xy -= unjitter;
        vec4 projected = g_shrd_data.view_from_clip * vec4(ray_vs, 1.0);
        ray_vs = projected.xyz / projected.w;
    }

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(ray_vs);
    float ray_length = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray of the same length "straight through" the reflecting surface and reprojecting the tip of that ray to the previous frame.
    ray_vs /= surface_depth; // == normalize(ray_vs)
    ray_vs *= ray_length;
    vec3 hit_position_ws; // This is the "fake" hit position if we would follow the ray straight through the surface.
    { // project from view space to world space
        // TODO: use matrix without translation here
        vec4 projected = g_shrd_data.world_from_view * vec4(ray_vs, 1.0);
        hit_position_ws = projected.xyz;
    }

    vec2 prev_hit_position;
    { // project to screen space of previous frame
        vec4 projected = g_shrd_data.prev_clip_from_world * vec4(hit_position_ws, 1.0);
        projected.xyz /= projected.w;
        projected.xy = 0.5 * projected.xy + 0.5;
#if defined(VULKAN)
        projected.y = (1.0 - projected.y);
#endif
        prev_hit_position = projected.xy;
    }
    return prev_hit_position;
}

float16_t GetDisocclusionFactor(const f16vec3 normal, const f16vec3 history_normal, float linear_depth, float history_linear_depth) {
    const float16_t factor = 1.0
                           * exp(-abs(1.0 - max(0.0, dot(normal, history_normal))) * DISOCCLUSION_NORMAL_WEIGHT)
                           * exp(-abs(history_linear_depth - linear_depth) / linear_depth * DISOCCLUSION_DEPTH_WEIGHT);
    return factor;
}

void PickReprojection(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size,
                      float16_t roughness, float16_t ray_len,
                      out float16_t disocclusion_factor, out vec2 reprojection_uv,
                      out f16vec4 reprojection) {
    moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);

    vec2 uv = (vec2(dispatch_thread_id) + vec2(0.5)) / vec2(screen_size);
    f16vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).xyz;
    f16vec3 history_normal;
    float history_linear_depth;

    {
        vec3 motion_vector = texelFetch(g_velocity_tex, ivec2(dispatch_thread_id), 0).xyz;
        motion_vector.xy /= g_shrd_data.res_and_fres.xy;
        const vec2 surf_repr_uv = uv - motion_vector.xy;
        const vec2 hit_repr_uv = GetHitPositionReprojection(ivec2(dispatch_thread_id), uv, ray_len);

        const f16vec4 surf_history = textureLod(g_refl_hist_tex, surf_repr_uv, 0.0);
        const f16vec4 hit_history = textureLod(g_refl_hist_tex, hit_repr_uv, 0.0);

        const f16vec4 surf_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, surf_repr_uv, 0.0).x);
        const f16vec4 hit_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, hit_repr_uv, 0.0).x);

        const f16vec3 surf_normal = surf_fetch.xyz, hit_normal = hit_fetch.xyz;
        const float16_t surf_roughness = surf_fetch.w, hit_roughness = hit_fetch.w;

        const float hit_normal_similarity = dot(hit_normal, normal);
        const float surf_normal_similarity = dot(surf_normal, normal);

        // Choose reprojection uv based on similarity to the local neighborhood
        if (hit_normal_similarity > REPROJECTION_NORMAL_SIMILARITY_THRESHOLD &&
            hit_normal_similarity + 1.0e-3 > surf_normal_similarity &&
            abs(hit_roughness - roughness) < abs(surf_roughness - roughness) + 1.0e-3) {
            // Mirror reflection
            history_normal = hit_normal;
            float hit_history_depth = textureLod(g_depth_hist_tex, hit_repr_uv, 0.0).x;
            history_linear_depth = LinearizeDepth(hit_history_depth, g_shrd_data.clip_info);
            reprojection_uv = hit_repr_uv;
            reprojection = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (length2(surf_history.xyz - local_neighborhood.mean.xyz) <
                REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                // Surface reflection
                history_normal = surf_normal;
                float surf_history_depth = textureLod(g_depth_hist_tex, surf_repr_uv, 0.0).x;
                history_linear_depth = LinearizeDepth(surf_history_depth, g_shrd_data.clip_info);
                reprojection_uv = surf_repr_uv;
                reprojection = surf_history;
            } else {
                disocclusion_factor = 0.0;
                return;
            }
        }

        const float depth = texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).x;
        const float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info) - motion_vector.z;
        // Determine disocclusion factor based on history
        disocclusion_factor = float(reprojection.w > 0.0);
        disocclusion_factor *= GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

        if (disocclusion_factor > DISOCCLUSION_THRESHOLD) {
            return;
        }

        // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            vec2 closest_uv = reprojection_uv;
            vec2 dudv = 1.0 / vec2(screen_size);

            const int SearchRadius = 1;
            for (int y = -SearchRadius; y <= SearchRadius; ++y) {
                for (int x = -SearchRadius; x <= SearchRadius; ++x) {
                    vec2 uv = reprojection_uv + vec2(x, y) * dudv;
                    f16vec3 history_normal = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, uv, 0.0).x).xyz;
                    float history_depth = textureLod(g_depth_hist_tex, uv, 0.0).x;
                    float history_linear_depth = LinearizeDepth(history_depth, g_shrd_data.clip_info);
                    float16_t weight = float(textureLod(g_refl_hist_tex, uv, 0.0).w > 0.0);
                    weight *= GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                    if (weight > disocclusion_factor) {
                        disocclusion_factor = weight;
                        closest_uv = uv;
                        reprojection_uv = closest_uv;
                    }
                }
            }
            reprojection = textureLod(g_refl_hist_tex, reprojection_uv, 0.0);
        }

        // Try to get rid of potential leaks at bilinear interpolation level
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
            // Helps quite a bit on the edges in movement
            const float uvx = fract(float(screen_size.x) * reprojection_uv.x + 0.5);
            const float uvy = fract(float(screen_size.y) * reprojection_uv.y + 0.5);
            const ivec2 reproject_texel_coords = ivec2(vec2(screen_size) * reprojection_uv - vec2(0.5));

            const f16vec4 reprojection00 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 0), 0);
            const f16vec4 reprojection10 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 0), 0);
            const f16vec4 reprojection01 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 1), 0);
            const f16vec4 reprojection11 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 1), 0);

            const f16vec3 normal00 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).x).xyz;
            const f16vec3 normal10 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).x).xyz;
            const f16vec3 normal01 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).x).xyz;
            const f16vec3 normal11 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).x).xyz;

            const float depth00 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).x, g_shrd_data.clip_info);
            const float depth10 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).x, g_shrd_data.clip_info);
            const float depth01 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).x, g_shrd_data.clip_info);
            const float depth11 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).x, g_shrd_data.clip_info);

            f16vec4 w;
            // Initialize with occlusion weights
            w.x = (reprojection00.w > 0.0 && GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > DISOCCLUSION_THRESHOLD / 2.0) ? 1.0 : 0.0;
            w.y = (reprojection10.w > 0.0 && GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > DISOCCLUSION_THRESHOLD / 2.0) ? 1.0 : 0.0;
            w.z = (reprojection01.w > 0.0 && GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > DISOCCLUSION_THRESHOLD / 2.0) ? 1.0 : 0.0;
            w.w = (reprojection11.w > 0.0 && GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > DISOCCLUSION_THRESHOLD / 2.0) ? 1.0 : 0.0;
            // And then mix in bilinear weights
            w.x = w.x * (1.0 - uvx) * (1.0 - uvy);
            w.y = w.y * (uvx) * (1.0 - uvy);
            w.z = w.z * (1.0 - uvx) * (uvy);
            w.w = w.w * (uvx) * (uvy);
            const float16_t ws = max(w.x + w.y + w.z + w.w, 1.0e-3);
            // normalize
            w /= ws;

            f16vec3 history_normal;
            float history_linear_depth;
            reprojection = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
            history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
            history_normal = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
            disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
        }
        disocclusion_factor = disocclusion_factor < DISOCCLUSION_THRESHOLD ? 0.0 : disocclusion_factor;
    }
}

void Reproject(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, int max_samples) {
    LoadIntoSharedMemory(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    float16_t variance = 1.0;
    const vec4 normal_fetch = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x);
    const vec3 normal = normal_fetch.xyz;
    const float roughness = normal_fetch.w;
    f16vec4 refl = texelFetch(g_refl_tex, ivec2(dispatch_thread_id), 0);

    const float depth = texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).x;
    const float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info);

    refl.w *= GetHitDistanceNormalization(linear_depth, roughness);

    if (refl.w > 0.0 && IsGlossyReflection(roughness)) {
        float16_t disocclusion_factor;
        vec2 reprojection_uv;
        f16vec4 reprojection;
        PickReprojection(ivec2(dispatch_thread_id), ivec2(group_thread_id), screen_size, roughness, refl.w,
                         disocclusion_factor, reprojection_uv, reprojection);

        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0)))) {
            const float16_t prev_variance = textureLod(g_variance_hist_tex, reprojection_uv, 0.0).x;
            float16_t sample_count = textureLod(g_sample_count_hist_tex, reprojection_uv, 0.0).x * disocclusion_factor;
            float16_t s_max_samples = max(8.0, max_samples * SAMPLES_FOR_ROUGHNESS(roughness));
            sample_count = min(s_max_samples, sample_count + 1);
            float16_t new_variance = ComputeTemporalVariance(refl.xyz, reprojection.xyz);
            if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
            } else {
                const float16_t variance_mix = mix(new_variance, prev_variance, 1.0 / sample_count);
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), reprojection);
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(variance_mix));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(sample_count));
                // Mix in reprojection for radiance mip computation
                refl = mix(refl, reprojection, 0.3);
            }
        } else {
            imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
            imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
            imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
        }
    } else {
        imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
        imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(0.0));
    }

    // Downsample 8x8 -> 1 radiance using shared memory
    // Initialize shared array for downsampling
    float16_t weight = float16_t(refl.w > 0.0);
    weight *= GetLuminanceWeight(refl.xyz);
    refl *= weight;
    if (any(greaterThanEqual(dispatch_thread_id, screen_size)) || any(isinf(refl)) || any(isnan(refl)) || weight > 1.0e3) {
        refl = vec4(0.0);
        weight = 0.0;
    }

    group_thread_id -= 4;

    g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(refl.xy);
    g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(vec2(refl.z, weight));

    groupMemoryBarrier();
    barrier();

    for (int i = 2; i <= 8; i = i * 2) {
        int ox = int(group_thread_id.x) * i;
        int oy = int(group_thread_id.y) * i;
        int ix = int(group_thread_id.x) * i + i / 2;
        int iy = int(group_thread_id.y) * i + i / 2;
        if (ix < 8 && iy < 8) {
            f16vec4 rad_weight00 = LoadFromSharedMemoryRaw(ivec2(ox, oy));
            f16vec4 rad_weight10 = LoadFromSharedMemoryRaw(ivec2(ox, iy));
            f16vec4 rad_weight01 = LoadFromSharedMemoryRaw(ivec2(ix, oy));
            f16vec4 rad_weight11 = LoadFromSharedMemoryRaw(ivec2(ix, iy));
            f16vec4 sum = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;

            g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.xy);
            g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.zw);
        }
        groupMemoryBarrier();
        barrier();
    }

    if (all(equal(group_thread_id, uvec2(0)))) {
        const f16vec4 sum = LoadFromSharedMemoryRaw(ivec2(0));
        const float16_t weight_acc = max(sum.w, 1.0e-3);
        const vec3 radiance_avg = (sum.xyz / weight_acc);

        imageStore(g_out_avg_refl_img, ivec2(dispatch_thread_id / 8), vec4(radiance_avg, 0.0));
    }
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, g_params.img_size, 32);
}
