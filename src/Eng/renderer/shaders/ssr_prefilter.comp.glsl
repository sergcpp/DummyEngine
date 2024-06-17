#version 430 core
#extension GL_ARB_shading_language_packing : require

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "ssr_prefilter_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = AVG_REFL_TEX_SLOT) uniform sampler2D g_avg_refl_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;
layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exp_tex;

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
    /* fp16 */ vec4 radiance;
    /* fp16 */ float variance;
    /* fp16 */ vec4 normal;
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
    variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;
    normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0).x);
    depth = texelFetch(g_depth_tex, dispatch_thread_id, 0).r;
}

void StoreWithOffset(ivec2 group_thread_id, const ivec2 _offset, const vec4 radiance, const float variance, const vec4 normal, const float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, radiance, variance, normal, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, const ivec2 group_thread_id, const float exposure) {
    // Load 16x16 region into shared memory.
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    /* fp16 */ vec4 radiance[4];
    /* fp16 */ float variance[4];
    /* fp16 */ vec4 normal[4];
    float depth[4];

    /// XA
    /// BC

    dispatch_thread_id -= 4; // 1 + 3 => additional band + left band

    for (int i = 0; i < 4; ++i) {
        LoadWithOffset(dispatch_thread_id, offset[i], radiance[i], variance[i], normal[i], depth[i]);
        radiance[i].rgb *= exposure;
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

// From https://github.com/GPUOpen-Effects/FidelityFX-Denoiser
/**********************************************************************
Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#define RADIANCE_WEIGHT_BIAS 0.6
#define RADIANCE_WEIGHT_VARIANCE_K 0.1
#define PREFILTER_VARIANCE_BIAS 0.1
#define PREFILTER_VARIANCE_WEIGHT 4.4

#define PREFILTER_NORMAL_SIGMA 65.0 // 512.0
#define PREFILTER_DEPTH_SIGMA 4.0

/* fp16 */ float GetEdgeStoppingNormalWeight(/* fp16 */ vec3 normal_p, /* fp16 */ vec3 normal_q) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), PREFILTER_NORMAL_SIGMA);
}

/* fp16 */ float GetEdgeStoppingDepthWeight(float center_depth, float neighbor_depth) {
    return exp(-abs(center_depth - neighbor_depth) * center_depth * PREFILTER_DEPTH_SIGMA);
}

/* fp16 */ float GetRadianceWeight(/* fp16 */ vec3 center_radiance, /* fp16 */ vec3 neighbor_radiance, /* fp16 */ float variance) {
    return max(exp(-(RADIANCE_WEIGHT_BIAS + variance * RADIANCE_WEIGHT_VARIANCE_K) * length(center_radiance - neighbor_radiance)), 1.0e-2);
}

void Resolve(ivec2 group_thread_id, /* fp16 */ vec3 avg_radiance, neighborhood_sample_t center,
             out /* fp16 */ vec4 resolved_radiance, out /* fp16 */ float resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    /* fp16 */ float accumulated_weight = GetRadianceWeight(avg_radiance, center.radiance.xyz, center.variance);
    /* fp16 */ vec4 accumulated_radiance = center.radiance * accumulated_weight;
    /* fp16 */ float accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    /* fp16 */ float variance_weight = max(PREFILTER_VARIANCE_BIAS, 1.0 - exp(-(center.variance * PREFILTER_VARIANCE_WEIGHT)));

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

        /* fp16 */ float weight = float(neighbor.radiance.w > 0.0);
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

void Prefilter(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float exposure) {
    InitSharedMemory(dispatch_thread_id, group_thread_id, exposure);

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    neighborhood_sample_t center = LoadFromSharedMemory(group_thread_id);

    /* fp16 */ vec4 resolved_radiance = center.radiance;
    /* fp16 */ float resolved_variance = center.variance;

    const bool needs_denoiser = (center.radiance.w > 0.0) && (center.variance > 0.0) && IsGlossyReflection(center.normal.w) && !IsMirrorReflection(center.normal.w);
    if (needs_denoiser) {
        const vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        /* fp16 */ const vec3 avg_radiance = textureLod(g_avg_refl_tex, uv8, 0.0).rgb * exposure;
        Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    imageStore(g_out_refl_img, dispatch_thread_id, vec4(resolved_radiance.xyz / exposure, resolved_radiance.w));
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(resolved_variance));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    const float exposure = texelFetch(g_exp_tex, ivec2(0), 0).x;

    Prefilter(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size.xy, exposure);
}
