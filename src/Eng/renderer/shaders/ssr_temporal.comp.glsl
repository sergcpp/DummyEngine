#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#endif

// NOTE: This is not used for now
#if !USE_FP16
    #define float16_t float
    #define f16vec2 vec2
    #define f16vec3 vec3
    #define f16vec4 vec4
#endif

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "taa_common.glsl"
#include "ssr_temporal_interface.h"

#pragma multi_compile _ RELAXED

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = AVG_REFL_TEX_SLOT) uniform sampler2D g_avg_refl_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = REPROJ_REFL_TEX_SLOT) uniform sampler2D g_reproj_refl_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_refl_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

#define LOCAL_NEIGHBORHOOD_RADIUS 4

#define GAUSSIAN_K 3.0

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

f16vec4 LoadFromSharedMemory(const ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
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
    ret.mean = f16vec4(0.0);
    ret.variance = f16vec4(0.0);

    float16_t accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            const ivec2 index = group_thread_id + ivec2(i, j);
            const f16vec4 radiance = LoadFromSharedMemory(index);
            const float16_t weight = LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
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

f16vec3 _ClipAABB(const f16vec3 aabb_min, const f16vec3 aabb_max, const f16vec3 prev_sample) {
    // Main idea behind clipping - it prevents clustering when neighbor color space
    // is distant from history sample

    // Here we find intersection between color vector and aabb color box

    // Note: only clips towards aabb center
    const f16vec3 aabb_center = 0.5 * (aabb_max + aabb_min);
    const f16vec3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    const f16vec3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    f16vec3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip       = abs(color_vector_clip);
    const float16_t max_abs_unit = max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0) {
        return aabb_center + color_vector / max_abs_unit; // clip towards color vector
    } else {
        return prev_sample; // point is inside aabb
    }
}

// Taken from https://github.com/GPUOpen-Effects/FidelityFX-Denoiser
void ResolveTemporal(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, vec2 inv_screen_size, float history_clip_weight) {
    LoadIntoSharedMemory(dispatch_thread_id, group_thread_id, ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // Center threads in shared memory

    f16vec4 center_radiance = LoadFromSharedMemory(group_thread_id);
    f16vec4 new_signal = center_radiance;
    float16_t roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0).x).w;
    float16_t new_variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;

    if (center_radiance.w > 0.0 && IsGlossyReflection(roughness) && !IsMirrorReflection(roughness)) {
        float16_t sample_count = (texelFetch(g_sample_count_tex, dispatch_thread_id, 0).x * MAX_SPECULAR_SAMPLES);
        const vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        f16vec3 avg_radiance = textureLod(g_avg_refl_tex, uv8, 0.0).xyz;

        f16vec4 old_signal = texelFetch(g_reproj_refl_tex, dispatch_thread_id, 0);
        moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);
        // Clip history based on the current local statistics
        f16vec3 color_std = (sqrt(local_neighborhood.variance.xyz) + length(local_neighborhood.mean.xyz - avg_radiance)) * history_clip_weight;
        local_neighborhood.mean.xyz = mix(local_neighborhood.mean.xyz, avg_radiance, 0.2);
        f16vec3 radiance_min = local_neighborhood.mean.xyz - color_std * 1.5;
        f16vec3 radiance_max = local_neighborhood.mean.xyz + color_std * 1.5;
        f16vec3 clipped_old_signal = _ClipAABB(radiance_min, radiance_max, old_signal.xyz);
        float16_t accumulation_speed = 1.0 / max(sample_count, 1.0);
        float16_t weight = (1.0 - accumulation_speed);
        // Blend with average for small sample count
        new_signal.xyz = mix(new_signal.xyz, avg_radiance, 1.0 / max(sample_count + 1.0, 1.0));
        { // Clip outliers
#ifdef RELAXED
            const f16vec3 radiance_min = avg_radiance - color_std * 0.75;
            const f16vec3 radiance_max = avg_radiance + color_std * 0.75;
#else
            const f16vec3 radiance_min = avg_radiance - color_std * 0.45;
            const f16vec3 radiance_max = avg_radiance + color_std * 0.45;
#endif
            new_signal.xyz = _ClipAABB(radiance_min, radiance_max, new_signal.xyz);
        }
        // Blend with history
        new_signal.xyz = mix(new_signal.xyz, clipped_old_signal.xyz, weight);
        new_signal.w = mix(new_signal.w, old_signal.w, weight);
        new_variance = mix(ComputeTemporalVariance(new_signal.xyz, clipped_old_signal), new_variance, weight);
    }

    imageStore(g_out_refl_img, dispatch_thread_id, sanitize(new_signal));
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(sanitize(new_variance)));
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    const vec2 inv_screen_size = 1.0 / vec2(g_params.img_size);

    ResolveTemporal(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size, inv_screen_size, 1.0 /* history_clip_weight */);
}
