#version 310 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "gi_common.glsl"
#include "gi_prefilter_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = AVG_GI_TEX_SLOT) uniform sampler2D g_avg_gi_tex;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_GI_IMG_SLOT, rgba16f) uniform image2D g_out_gi_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

shared uint g_shared_0[16][16];
shared uint g_shared_1[16][16];
shared uint g_shared_2[16][16];
shared uint g_shared_3[16][16];
shared float g_shared_depth[16][16];

struct sample_t {
    /* mediump */ vec4 radiance;
    /* mediump */ float variance;
    /* mediump */ vec3 normal;
    float depth;
};

sample_t LoadFromSharedMemory(ivec2 idx) {
    sample_t ret;
    ret.radiance.xy = unpackHalf2x16(g_shared_0[idx.y][idx.x]);
    ret.radiance.zw = unpackHalf2x16(g_shared_1[idx.y][idx.x]);
    ret.normal.xy = unpackHalf2x16(g_shared_2[idx.y][idx.x]);
    /* mediump */ vec2 temp = unpackHalf2x16(g_shared_3[idx.y][idx.x]);
    ret.normal.z = temp.x;
    ret.variance = temp.y;
    ret.depth = g_shared_depth[idx.y][idx.x];
    return ret;
}

void StoreInSharedMemory(ivec2 idx, vec4 radiance, float variance, vec3 normal, float depth) {
    g_shared_0[idx.y][idx.x] = packHalf2x16(radiance.xy);
    g_shared_1[idx.y][idx.x] = packHalf2x16(radiance.zw);
    g_shared_2[idx.y][idx.x] = packHalf2x16(normal.xy);
    g_shared_3[idx.y][idx.x] = packHalf2x16(vec2(normal.z, variance));
    g_shared_depth[idx.y][idx.x] = depth;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 _offset,
                    out vec4 radiance, out float variance, out vec3 normal, out float depth) {
    dispatch_thread_id += _offset;
    radiance = texelFetch(g_gi_tex, dispatch_thread_id, 0);
    variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;
    normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0)).xyz;
    depth = texelFetch(g_depth_tex, dispatch_thread_id, 0).r;
}

void StoreWithOffset(ivec2 group_thread_id, ivec2 _offset, vec4 radiance, float variance, vec3 normal, float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, radiance, variance, normal, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id) {
    // Load 16x16 region into shared memory.
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    /* mediump */ vec4 radiance[4];
    /* mediump */ float variance[4];
    /* mediump */ vec3 normal[4];
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

#define PREFILTER_NORMAL_SIGMA 4.0 // 512.0
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

float GetGaussianWeight(float r) {
    // radius is normalized to 1
    return exp( -0.66 * r * r );
}

void Resolve(ivec2 group_thread_id, /* mediump */ vec3 avg_radiance, sample_t center,
             out /* mediump */ vec4 resolved_radiance, out /* mediump */ float resolved_variance) {
    // Initial weight is important to remove fireflies.
    // That removes quite a bit of energy but makes everything much more stable.
    /* mediump */ float accumulated_weight = GetRadianceWeight(avg_radiance, center.radiance.rgb, center.variance);
    /* mediump */ vec4 accumulated_radiance = center.radiance * accumulated_weight;
    /* mediump */ float accumulated_variance = center.variance * accumulated_weight * accumulated_weight;
    /* mediump */ float variance_weight = max(PREFILTER_VARIANCE_BIAS, 1.0 - exp(-(center.variance * PREFILTER_VARIANCE_WEIGHT)));

    for (int i = 0; i < 16; ++i) {
        ivec2 new_idx = group_thread_id + ivec2(RotateVector(g_params.rotator, 4.0 * g_Poisson16[i].xy));
        sample_t neighbor = LoadFromSharedMemory(new_idx);

        /* mediump */ float weight = GetGaussianWeight(g_Poisson16[i].z);
        weight *= GetEdgeStoppingNormalWeight(center.normal, neighbor.normal);
        weight *= GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);
        weight *= GetRadianceWeight(avg_radiance, neighbor.radiance.rgb, center.variance);
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

    sample_t center = LoadFromSharedMemory(group_thread_id);

    /* mediump */ vec4 resolved_radiance = center.radiance;
    /* mediump */ float resolved_variance = center.variance;

    bool needs_denoiser = center.variance > 0.0 /*&& IsGlossyReflection(center.normal.w) && !IsMirrorReflection(center.normal.w)*/;
    if (needs_denoiser) {
        vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        /* mediump */ vec3 avg_radiance = textureLod(g_avg_gi_tex, uv8, 0.0).rgb;
        Resolve(group_thread_id, avg_radiance, center, resolved_radiance, resolved_variance);
    }

    imageStore(g_out_gi_img, dispatch_thread_id, resolved_radiance);
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
