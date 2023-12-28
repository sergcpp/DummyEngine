#version 310 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "ssr_prefilter_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = AVG_REFL_TEX_SLOT) uniform sampler2D g_avg_refl_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REFL_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_refl_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

shared uint g_shared_0[16][16];
shared uint g_shared_1[16][16];
shared uint g_shared_2[16][16];
shared uint g_shared_3[16][16];
shared float g_shared_depth[16][16];

struct neighborhood_sample_t {
    /* mediump */ vec3 radiance;
    /* mediump */ float variance;
    /* mediump */ vec4 normal;
    float depth;
};

neighborhood_sample_t LoadFromSharedMemory(ivec2 idx) {
    neighborhood_sample_t ret;
    ret.radiance.xy = unpackHalf2x16(g_shared_0[idx.y][idx.x]);
    /* mediump */ vec2 temp = unpackHalf2x16(g_shared_1[idx.y][idx.x]);
    ret.radiance.z = temp.x;
    ret.variance = temp.y;
    ret.normal.xy = unpackHalf2x16(g_shared_2[idx.y][idx.x]);
    ret.normal.zw = unpackHalf2x16(g_shared_3[idx.y][idx.x]);
    ret.depth = g_shared_depth[idx.y][idx.x];
    return ret;
}

void StoreInSharedMemory(ivec2 idx, vec3 radiance, float variance, vec4 normal, float depth) {
    g_shared_0[idx.y][idx.x] = packHalf2x16(radiance.xy);
    g_shared_1[idx.y][idx.x] = packHalf2x16(vec2(radiance.z, variance));
    g_shared_2[idx.y][idx.x] = packHalf2x16(normal.xy);
    g_shared_3[idx.y][idx.x] = packHalf2x16(normal.zw);
    g_shared_depth[idx.y][idx.x] = depth;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 _offset,
                    out vec3 radiance, out float variance, out vec4 normal, out float depth) {
    dispatch_thread_id += _offset;
    radiance = texelFetch(g_refl_tex, dispatch_thread_id, 0).rgb;
    variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;
    normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0));
    depth = texelFetch(g_depth_tex, dispatch_thread_id, 0).r;
}

void StoreWithOffset(ivec2 group_thread_id, ivec2 _offset, vec3 radiance, float variance, vec4 normal, float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, radiance, variance, normal, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id) {
    // Load 16x16 region into shared memory.
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    /* mediump */ vec3 radiance[4];
    /* mediump */ float variance[4];
    /* mediump */ vec4 normal[4];
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

bool IsGlossyReflection(float roughness) {
    return roughness < g_params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < g_params.thresholds.y; //0.0001;
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

/* mediump */ float GetEdgeStoppingNormalWeight(/* mediump */ vec3 normal_p, /* mediump */ vec3 normal_q) {
    return pow(clamp(dot(normal_p, normal_q), 0.0, 1.0), PREFILTER_NORMAL_SIGMA);
}

/* mediump */ float GetEdgeStoppingDepthWeight(float center_depth, float neighbor_depth) {
    return exp(-abs(center_depth - neighbor_depth) * center_depth * PREFILTER_DEPTH_SIGMA);
}

/* mediump */ float GetRadianceWeight(/* mediump */ vec3 center_radiance, /* mediump */ vec3 neighbor_radiance, /* mediump */ float variance) {
    return max(exp(-(RADIANCE_WEIGHT_BIAS + variance * RADIANCE_WEIGHT_VARIANCE_K) * length(center_radiance - neighbor_radiance)), 1.0e-2);
}

void Resolve(ivec2 group_thread_id, /* mediump */ vec3 avg_radiance, neighborhood_sample_t center,
             out /* mediump */ vec3 resolved_radiance, out /* mediump */ float resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    /* mediump */ float accumulated_weight = GetRadianceWeight(avg_radiance, center.radiance, center.variance);
    /* mediump */ vec3 accumulated_radiance = center.radiance * accumulated_weight;
    /* mediump */ float accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    // First 15 numbers of Halton(2, 3) streteched to [-3, 3]
    const uint sample_count = 15;
    const ivec2 sample_offsets[] = {
        ivec2(0, 1),
        ivec2(-2, 1),
        ivec2(2, -3),
        ivec2(-3, 0),
        ivec2(1, 2),
        ivec2(-1, -2),
        ivec2(3, 0),
        ivec2(-3, 3),
        ivec2(0, -3),
        ivec2(-1, -1),
        ivec2(2, 1),
        ivec2(-2, -2),
        ivec2(1, 0),
        ivec2(0, 2),
        ivec2(3, -1)
    };
    /* mediump */ float variance_weight = max(PREFILTER_VARIANCE_BIAS, 1.0 - exp(-(center.variance * PREFILTER_VARIANCE_WEIGHT)));

    for (int i = 0; i < sample_count; ++i) {
        ivec2 new_idx = group_thread_id + sample_offsets[i];
        neighborhood_sample_t neighbor = LoadFromSharedMemory(new_idx);

        /* mediump */ float weight = 1.0;
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

    /* mediump */ vec3 resolved_radiance = center.radiance;
    /* mediump */ float resolved_variance = center.variance;

    bool needs_denoiser = center.variance > 0.0 && IsGlossyReflection(center.normal.w) && !IsMirrorReflection(center.normal.w);
    if (needs_denoiser) {
        vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        /* mediump */ vec3 avg_radiance = textureLod(g_avg_refl_tex, uv8, 0.0).rgb;
        Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    imageStore(g_out_refl_img, dispatch_thread_id, vec4(resolved_radiance, 1.0));
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(resolved_variance));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    ivec2  dispatch_group_id = dispatch_thread_id / 8;
    uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Prefilter(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size.xy);
}
