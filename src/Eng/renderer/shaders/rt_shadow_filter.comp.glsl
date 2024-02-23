#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "rt_shadow_filter_interface.h"
#include "rt_shadow_common.glsl.inl"

/*
PERM @PASS_0
PERM @PASS_1
*/

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(binding = INPUT_TEX_SLOT) uniform sampler2D g_input_tex;

layout(std430, binding = TILE_METADATA_BUF_SLOT) readonly buffer TileMetadata {
    uint g_tile_metadata[];
};

#if defined(PASS_0) || defined(PASS_1)
layout(binding = OUT_RESULT_IMG_SLOT, rg16f) uniform writeonly image2D g_out_result_img;
#else
layout(binding = OUT_RESULT_IMG_SLOT, r8) uniform writeonly image2D g_out_result_img;
#endif

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

shared uint g_shared_input[16][16];
shared float g_shared_depth[16][16];
shared uint g_shared_normals_xy[16][16];
shared uint g_shared_normals_zw[16][16];

void LoadWithOffset(ivec2 did, ivec2 offset, out /* mediump */ vec3 normals, out /* mediump */ vec2 input_, out float depth) {
    did += offset;

    ivec2 p = clamp(did, ivec2(0, 0), ivec2(g_params.img_size) - ivec2(1));
    normals = UnpackNormalAndRoughness(texelFetch(g_norm_tex, p, 0).x).xyz;
    input_ = texelFetch(g_input_tex, p, 0).rg;
    depth = texelFetch(g_depth_tex, p, 0).r;
}

/* mediump */ vec2 LoadInputFromSharedMemory(ivec2 idx) {
    return unpackHalf2x16(g_shared_input[idx.y][idx.x]);
}

float LoadDepthFromSharedMemory(ivec2 idx) {
    return g_shared_depth[idx.y][idx.x];
}

/* mediump */ vec3 LoadNormalsFromSharedMemory(ivec2 idx) {
    vec3 normals;
    normals.xy = unpackHalf2x16(g_shared_normals_xy[idx.y][idx.x]);
    normals.z = unpackHalf2x16(g_shared_normals_zw[idx.y][idx.x]).x;
    return normals;
}

float FetchFilteredVarianceFromSharedMemory(ivec2 pos) {
    const int k = 1;
    float variance = 0.0;
    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };
    for (int y = -k; y <= k; ++y) {
        for (int x = -k; x <= k; ++x) {
            const float w = kernel[abs(x)][abs(y)];
            variance += w * LoadInputFromSharedMemory(pos + ivec2(x, y)).y;
        }
    }
    return variance;
}

void StoreInGroupSharedMemory(ivec2 idx, /* mediump */ vec3 normals, /* mediump */ vec2 input_, float depth) {
    g_shared_input[idx.y][idx.x] = packHalf2x16(input_);
    g_shared_depth[idx.y][idx.x] = depth;
    g_shared_normals_xy[idx.y][idx.x] = packHalf2x16(normals.xy);
    g_shared_normals_zw[idx.y][idx.x] = packHalf2x16(vec2(normals.z, 0.0));
}

void StoreWithOffset(ivec2 gtid, ivec2 offset, /* mediump */ vec3 normals, /* mediump */ vec2 input_, float depth) {
    gtid += offset;
    StoreInGroupSharedMemory(gtid, normals, input_, depth);
}

void InitializeSharedMemory(ivec2 did, ivec2 gtid) {
    const ivec2 offset_0 = ivec2(0, 0);
    const ivec2 offset_1 = ivec2(8, 0);
    const ivec2 offset_2 = ivec2(0, 8);
    const ivec2 offset_3 = ivec2(8, 8);

    /* mediump */ vec3 normals_0;
    /* mediump */ vec2 input_0;
    float depth_0;

    /* mediump */ vec3 normals_1;
    /* mediump */ vec2 input_1;
    float depth_1;

    /* mediump */ vec3 normals_2;
    /* mediump */ vec2 input_2;
    float depth_2;

    /* mediump */ vec3 normals_3;
    /* mediump */ vec2 input_3;
    float depth_3;

    /// XA
    /// BC

    did -= 4;
    LoadWithOffset(did, offset_0, normals_0, input_0, depth_0); // X
    LoadWithOffset(did, offset_1, normals_1, input_1, depth_1); // A
    LoadWithOffset(did, offset_2, normals_2, input_2, depth_2); // B
    LoadWithOffset(did, offset_3, normals_3, input_3, depth_3); // C

    StoreWithOffset(gtid, offset_0, normals_0, input_0, depth_0); // X
    StoreWithOffset(gtid, offset_1, normals_1, input_1, depth_1); // A
    StoreWithOffset(gtid, offset_2, normals_2, input_2, depth_2); // B
    StoreWithOffset(gtid, offset_3, normals_3, input_3, depth_3); // C
}

bool IsShadowReceiver(uvec2 p) {
    float depth = texelFetch(g_depth_tex, ivec2(p), 0).r;
    return (depth > 0.0) && (depth < 1.0);
}

float GetShadowSimilarity(float x1, float x2, float sigma) {
    return exp(-abs(x1 - x2) / sigma);
}

float GetDepthSimilarity(float x1, float x2, float sigma) {
    return exp(-abs(x1 - x2) / sigma);
}

float GetNormalSimilarity(vec3 x1, vec3 x2) {
    return pow(clamp(dot(x1, x2), 0.0, 1.0), 32.0);
}

const float DepthSimilaritySigma = 1.0;

void DenoiseFromSharedMemory(uvec2 did, uvec2 gtid, inout float weight_sum, inout vec2 shadow_sum, float depth, uint stepsize) {
    // Load our center sample
    vec2 shadow_center = LoadInputFromSharedMemory(ivec2(gtid));
    vec3 normal_center = LoadNormalsFromSharedMemory(ivec2(gtid));

    weight_sum = 1.0;
    shadow_sum = shadow_center;

    const float variance = FetchFilteredVarianceFromSharedMemory(ivec2(gtid));
    const float std_deviation = sqrt(max(variance + 1e-9, 0.0));
    const float depth_center = LinearizeDepth(depth, g_shrd_data.clip_info);

    // Iterate filter kernel
    const int k = 1;
    const float kernel[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    for (int y = -k; y <= k; ++y) {
        for (int x = -k; x <= k; ++x) {
            // Should we process this sample?
            const ivec2 _step = ivec2(x, y) * int(stepsize);
            const ivec2 gtid_idx = ivec2(gtid) + _step;
            const ivec2 did_idx = ivec2(did) + _step;

            float depth_neigh = LoadDepthFromSharedMemory(gtid_idx);
            vec3 normal_neigh = LoadNormalsFromSharedMemory(gtid_idx);
            vec2 shadow_neigh = LoadInputFromSharedMemory(gtid_idx);

            float sky_pixel_multiplier = ((x == 0 && y == 0) || depth_neigh >= 1.0 || depth_neigh <= 0.0) ? 0.0 : 1.0; // Zero weight for sky pixels

            // Fetch our filtering values
            depth_neigh = LinearizeDepth(depth_neigh, g_shrd_data.clip_info);

            // Evaluate the edge-stopping function
            float w = kernel[abs(x)] * kernel[abs(y)];  // kernel weight
            w *= GetShadowSimilarity(shadow_center.x, shadow_neigh.x, std_deviation);
            w *= GetDepthSimilarity(depth_center, depth_neigh, DepthSimilaritySigma);
            w *= GetNormalSimilarity(normal_center, normal_neigh);
            w *= sky_pixel_multiplier;

            // Accumulate the filtered sample
            shadow_sum += vec2(w, w * w) * shadow_neigh;
            weight_sum += w;
        }
    }
}

vec2 ApplyFilterWithPrecache(uvec2 did, uvec2 gtid, uint stepsize) {
    float weight_sum = 1.0;
    vec2 shadow_sum = vec2(0.0);

    InitializeSharedMemory(ivec2(did), ivec2(gtid));
    bool needs_denoiser = IsShadowReceiver(did);
    groupMemoryBarrier(); barrier();
    if (needs_denoiser) {
        float depth = texelFetch(g_depth_tex, ivec2(did), 0).r;
        gtid += 4; // Center threads in groupshared memory
        DenoiseFromSharedMemory(did, gtid, weight_sum, shadow_sum, depth, stepsize);
    }

    float mean = shadow_sum.x / weight_sum;
    float variance = shadow_sum.y / (weight_sum * weight_sum);
    return vec2(mean, variance);
}


void ReadTileMetaData(uvec2 gid, out bool is_cleared, out bool all_in_light) {
    uint meta_data = g_tile_metadata[gid.y * ((g_params.img_size.x + 7) / 8) + gid.x];
    is_cleared = (meta_data & TILE_META_DATA_CLEAR_MASK) != 0u;
    all_in_light = (meta_data & TILE_META_DATA_LIGHT_MASK) != 0u;
}

vec2 FilterSoftShadowsPass(uvec2 gid, uvec2 gtid, uvec2 did, out bool bWriteResults, uint pass, uint stepsize) {
    bool is_cleared;
    bool all_in_light;
    ReadTileMetaData(gid, is_cleared, all_in_light);

    bWriteResults = false;
    vec2 results = vec2(0.0);
    // [branch]
    if (is_cleared) {
        if (pass != 1) {
            results.x = all_in_light ? 1.0 : 0.0;
            bWriteResults = true;
        }
    } else {
        results = ApplyFilterWithPrecache(did, gtid, stepsize);
        bWriteResults = true;
    }

    return results;
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uvec2 group_thread_id = gl_LocalInvocationID.xy;
    uvec2 dispatch_thread_id = gl_GlobalInvocationID.xy;

#if defined(PASS_0)
    const uint PASS_INDEX = 0u;
    const uint STEP_SIZE = 1u;

    bool bWriteOutput = false;
    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, bWriteOutput, PASS_INDEX, STEP_SIZE);
#elif defined(PASS_1)
    const uint PASS_INDEX = 1u;
    const uint STEP_SIZE = 2u;

    bool bWriteOutput = false;
    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, bWriteOutput, PASS_INDEX, STEP_SIZE);
#else
    const uint PASS_INDEX = 2u;
    const uint STEP_SIZE = 4u;

    bool bWriteOutput = false;
    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, bWriteOutput, PASS_INDEX, STEP_SIZE);

    // Recover some of the contrast lost during denoising
    const float shadow_remap = max(1.2f - results.y, 1.0f);
    const float mean = pow(abs(results.x), shadow_remap);
#endif

    if (bWriteOutput) {
#if defined(PASS_0) || defined(PASS_1)
        imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(results, 0.0, 0.0));
#else
        imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(mean, 0.0, 0.0, 0.0));
#endif
    }
}
