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
#include "ssr_common.glsl"
#include "ssr_prefilter_interface.h"

#pragma multi_compile _ RELAXED

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = AVG_REFL_TEX_SLOT) uniform sampler2D g_avg_refl_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_refl_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

shared uint g_shared_0[16][16];
shared uint g_shared_1[16][16];
shared uint g_shared_2[16][16];
shared uint g_shared_3[16][16];
shared uint g_shared_4[16][16];
shared float g_shared_depth[16][16];

struct neighborhood_sample_t {
    f16vec4 radiance;
    float16_t variance;
    f16vec4 normal;
    float depth;
};

neighborhood_sample_t LoadFromSharedMemory(ivec2 idx) {
    neighborhood_sample_t ret;
    ret.radiance.xy = unpackHalf2x16(g_shared_0[idx.y][idx.x]);
    ret.radiance.zw = unpackHalf2x16(g_shared_1[idx.y][idx.x]);
    ret.variance = unpackHalf2x16(g_shared_2[idx.y][idx.x]).x;
    ret.normal.xy = unpackHalf2x16(g_shared_3[idx.y][idx.x]);
    ret.normal.zw = unpackHalf2x16(g_shared_4[idx.y][idx.x]);
    ret.depth = g_shared_depth[idx.y][idx.x];
    return ret;
}

void StoreInSharedMemory(const ivec2 idx, const vec4 radiance, const float variance, const vec4 normal, const float depth) {
    g_shared_0[idx.y][idx.x] = packHalf2x16(radiance.xy);
    g_shared_1[idx.y][idx.x] = packHalf2x16(radiance.zw);
    g_shared_2[idx.y][idx.x] = packHalf2x16(vec2(variance, 0.0));
    g_shared_3[idx.y][idx.x] = packHalf2x16(normal.xy);
    g_shared_4[idx.y][idx.x] = packHalf2x16(normal.zw);
    g_shared_depth[idx.y][idx.x] = depth;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 _offset,
                    out vec4 radiance, out float variance, out vec4 normal, out float depth) {
    dispatch_thread_id += _offset;
    radiance = texelFetch(g_refl_tex, dispatch_thread_id, 0);
    variance = sanitize(texelFetch(g_variance_tex, dispatch_thread_id, 0).x);
    normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0).x);
    depth = 1.0 - texelFetch(g_depth_tex, dispatch_thread_id, 0).x;
}

void StoreWithOffset(ivec2 group_thread_id, const ivec2 _offset, const vec4 radiance, const float variance, const vec4 normal, const float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, radiance, variance, normal, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, const ivec2 group_thread_id) {
    // Load 16x16 region into shared memory.
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    f16vec4 radiance[4];
    float16_t variance[4];
    f16vec4 normal[4];
    float depth[4];

    /// XA
    /// BC

    dispatch_thread_id -= 4; // 1 + 3 => additional band + left band

    for (int i = 0; i < 4; ++i) {
        LoadWithOffset(dispatch_thread_id, offset[i], radiance[i], variance[i], normal[i], depth[i]);
    }

    for (int i = 0; i < 4; ++i) {
        StoreWithOffset(group_thread_id, offset[i], radiance[i], variance[i], normal[i], depth[i]);
    }
}

float Gaussian(float x, float m, float sigma) {
    float a = length(x - m) / sigma;
    a *= a;
    return exp(-0.5 * a);
}

// Taken from https://github.com/GPUOpen-Effects/FidelityFX-Denoiser

#define RADIANCE_WEIGHT_BIAS 0.0
#define RADIANCE_WEIGHT_VARIANCE_K 0.02
#define PREFILTER_VARIANCE_BIAS 0.1
#define PREFILTER_VARIANCE_WEIGHT 4.4

#define PREFILTER_NORMAL_SIGMA 65.0 // 512.0
#define PREFILTER_DEPTH_SIGMA 4.0

float16_t GetEdgeStoppingNormalWeight(const f16vec3 normal_p, const f16vec3 normal_q) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), PREFILTER_NORMAL_SIGMA);
}

float16_t GetEdgeStoppingDepthWeight(float center_depth, float neighbor_depth) {
    return exp(-abs(center_depth - neighbor_depth) * center_depth * PREFILTER_DEPTH_SIGMA);
}

float16_t GetRadianceWeight(const f16vec3 center_radiance, const f16vec3 neighbor_radiance, const float16_t variance) {
#ifndef RELAXED
    return max(exp(-(RADIANCE_WEIGHT_BIAS + variance * RADIANCE_WEIGHT_VARIANCE_K) * length(center_radiance - neighbor_radiance)), 1.0e-2);
#else
    return 1.0;
#endif
}

void Resolve(ivec2 group_thread_id, f16vec3 avg_radiance, neighborhood_sample_t center,
             out f16vec4 resolved_radiance, out float16_t resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    float16_t accumulated_weight = GetRadianceWeight(avg_radiance, center.radiance.xyz, center.variance);
    f16vec4 accumulated_radiance = center.radiance * accumulated_weight;
    float16_t accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    float16_t variance_weight = max(PREFILTER_VARIANCE_BIAS, 1.0 - exp(-(center.variance * PREFILTER_VARIANCE_WEIGHT)));

    const ivec2 sample_offsets[] = {
        ivec2(-1,  0),
        ivec2( 1,  0),
        ivec2( 0, -1),
        ivec2( 0,  1),
        ivec2(-2,  2),
        ivec2( 2,  2),
        ivec2(-2, -2),
        ivec2( 2, -2),
        ivec2(-3,  0),
        ivec2( 3,  0),
        ivec2( 0, -3),
        ivec2( 0,  3)
    };

    for (int i = 0; i < 12; ++i) {
        const ivec2 new_idx = group_thread_id + sample_offsets[i];
        const neighborhood_sample_t neighbor = LoadFromSharedMemory(new_idx);

        float16_t weight = float16_t(neighbor.radiance.w > 0.0);
        weight *= GetEdgeStoppingNormalWeight(center.normal.xyz, neighbor.normal.xyz);
        weight *= GetEdgeStoppingRoughnessWeight(center.normal.w, neighbor.normal.w, RoughnessSigmaMin, RoughnessSigmaMax);
        weight *= GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);
        weight *= GetRadianceWeight(avg_radiance, neighbor.radiance.xyz, center.variance);
        weight *= variance_weight;

        // Accumulate all contributions
        accumulated_weight += weight;
        accumulated_radiance += weight * neighbor.radiance;
        accumulated_variance += weight * weight * neighbor.variance;
    }

    accumulated_radiance /= accumulated_weight;
    accumulated_variance /= (accumulated_weight * accumulated_weight);

    resolved_radiance = accumulated_radiance;
    resolved_variance = accumulated_variance;
}

void Prefilter(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size) {
    InitSharedMemory(dispatch_thread_id, group_thread_id);

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    neighborhood_sample_t center = LoadFromSharedMemory(group_thread_id);

    f16vec4 resolved_radiance = center.radiance;
    float16_t resolved_variance = center.variance;

    const bool needs_denoiser = (center.radiance.w > 0.0) && (center.variance > 0.0) && IsGlossyReflection(center.normal.w) && !IsMirrorReflection(center.normal.w);
    if (needs_denoiser) {
        const vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        const f16vec3 avg_radiance = textureLod(g_avg_refl_tex, uv8, 0.0).xyz;
        Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    imageStore(g_out_refl_img, dispatch_thread_id, resolved_radiance);
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(resolved_variance));
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Prefilter(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size.xy);
}
