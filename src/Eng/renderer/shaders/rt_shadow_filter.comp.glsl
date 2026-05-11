#version 430 core
#extension GL_EXT_control_flow_attributes : enable

#include "_cs_common.glsl"
#include "rt_shadow_filter_interface.h"
#include "rt_shadow_common.glsl.inl"

#pragma multi_compile _ PASS_0 PASS_1

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
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

layout(binding = OUT_RESULT_IMG_SLOT, rg16f) uniform restrict writeonly image2D g_out_result_img;

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

void LoadWithOffset(ivec2 did, const ivec2 offset, out /* fp16 */ vec3 normals, out /* fp16 */ vec2 input_, out float depth) {
    did += offset;

    const ivec2 p = clamp(did, ivec2(0, 0), ivec2(g_params.img_size) - ivec2(1));
    normals = UnpackNormalAndRoughness(texelFetch(g_norm_tex, p, 0).x).xyz;
    input_ = texelFetch(g_input_tex, p, 0).xy;
    depth = texelFetch(g_depth_tex, p, 0).x;
    if (depth > 0.0 && depth < 1.0)
    {
        depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    }
    else
    {
        depth = 0.0;
    }
}

/* fp16 */ vec2 LoadInputFromSharedMemory(const ivec2 idx) {
    return unpackHalf2x16(g_shared_input[idx.y][idx.x]);
}

float LoadDepthFromSharedMemory(const ivec2 idx) {
    return g_shared_depth[idx.y][idx.x];
}

/* fp16 */ vec3 LoadNormalsFromSharedMemory(const ivec2 idx) {
    vec3 normals;
    normals.xy = unpackHalf2x16(g_shared_normals_xy[idx.y][idx.x]);
    normals.z = unpackHalf2x16(g_shared_normals_zw[idx.y][idx.x]).x;
    return normals;
}

float FetchFilteredVarianceFromSharedMemory(const ivec2 pos) {
    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };
    float variance = 0.0;
    [[unroll]] for (int y = -1; y <= 1; ++y) {
        [[unroll]] for (int x = -1; x <= 1; ++x) {
            const float w = kernel[abs(x)][abs(y)];
            variance += w * LoadInputFromSharedMemory(pos + ivec2(x, y)).y;
        }
    }
    return variance;
}

void StoreInSharedMemory(const ivec2 idx, const /* fp16 */ vec3 normals, const /* fp16 */ vec2 input_, const float depth) {
    g_shared_input[idx.y][idx.x] = packHalf2x16(input_);
    g_shared_depth[idx.y][idx.x] = depth;
    g_shared_normals_xy[idx.y][idx.x] = packHalf2x16(normals.xy);
    g_shared_normals_zw[idx.y][idx.x] = packHalf2x16(vec2(normals.z, 0.0));
}

void StoreWithOffset(ivec2 gtid, ivec2 offset, /* fp16 */ vec3 normals, /* fp16 */ vec2 input_, float depth) {
    gtid += offset;
    StoreInSharedMemory(gtid, normals, input_, depth);
}

void InitializeSharedMemory(ivec2 did, ivec2 gtid) {
    const ivec2 offset_0 = ivec2(0, 0);
    const ivec2 offset_1 = ivec2(8, 0);
    const ivec2 offset_2 = ivec2(0, 8);
    const ivec2 offset_3 = ivec2(8, 8);

    /* fp16 */ vec3 normals_0;
    /* fp16 */ vec2 input_0;
    float depth_0;

    /* fp16 */ vec3 normals_1;
    /* fp16 */ vec2 input_1;
    float depth_1;

    /* fp16 */ vec3 normals_2;
    /* fp16 */ vec2 input_2;
    float depth_2;

    /* fp16 */ vec3 normals_3;
    /* fp16 */ vec2 input_3;
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

bool IsShadowReceiver(const uvec2 p) {
    const float depth = texelFetch(g_depth_tex, ivec2(p), 0).x;
    return (depth > 0.0) && (depth < 1.0);
}

float GetShadowSimilarity(const float x1, const float x2, const float sigma) {
    return exp(-abs(x1 - x2) / sigma);
}

float GetDepthSimilarity(const float center_depth, const float neighbor_depth, const float sigma) {
    return exp(-abs(center_depth - neighbor_depth) * center_depth * sigma);
}

float GetNormalSimilarity(const vec3 x1, const vec3 x2) {
    return pow(saturate(dot(x1, x2)), 32.0);
}

const float DepthSimilaritySigma = 1024.0;

void DenoiseFromSharedMemory(const uvec2 did, const uvec2 gtid, inout float weight_sum, inout vec2 shadow_sum, const float depth, const uint stepsize) {
    // Load our center sample
    const vec2 shadow_center = LoadInputFromSharedMemory(ivec2(gtid));
    const vec3 normal_center = LoadNormalsFromSharedMemory(ivec2(gtid));

    weight_sum = 1.0;
    shadow_sum = shadow_center;

    const float variance = FetchFilteredVarianceFromSharedMemory(ivec2(gtid));
    const float std_deviation = sqrt(max(variance + 1e-9, 0.0));
    const float depth_center = LinearizeDepth(depth, g_shrd_data.clip_info);

    // Iterate filter kernel
    const float kernel[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    [[unroll]] for (int y = -1; y <= 1; ++y) {
        [[unroll]] for (int x = -1; x <= 1; ++x) {
            const ivec2 _step = ivec2(x, y) * int(stepsize);
            const ivec2 gtid_idx = ivec2(gtid) + _step;

            const float depth_neigh = LoadDepthFromSharedMemory(gtid_idx);
            const vec3 normal_neigh = LoadNormalsFromSharedMemory(gtid_idx);
            const vec2 shadow_neigh = LoadInputFromSharedMemory(gtid_idx);

            // This also skips center
            const float sky_pixel_multiplier = ((x == 0 && y == 0) || depth_neigh == 0.0) ? 0.0 : 1.0;

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
        float depth = texelFetch(g_depth_tex, ivec2(did), 0).x;
        gtid += 4; // Center threads in shared memory
        DenoiseFromSharedMemory(did, gtid, weight_sum, shadow_sum, depth, stepsize);
    }

    const float mean = shadow_sum.x / weight_sum;
    const float variance = shadow_sum.y / (weight_sum * weight_sum);
    return vec2(mean, variance);
}


void ReadTileMetaData(uvec2 gid, out bool is_cleared, out bool all_in_light) {
    uint meta_data = g_tile_metadata[gid.y * ((g_params.img_size.x + 7) / 8) + gid.x];
    is_cleared = (meta_data & TILE_META_DATA_CLEAR_MASK) != 0u;
    all_in_light = (meta_data & TILE_META_DATA_LIGHT_MASK) != 0u;
}

vec2 FilterSoftShadowsPass(uvec2 gid, uvec2 gtid, uvec2 did, out bool write_results, uint pass, uint stepsize) {
    bool is_cleared, all_in_light;
    ReadTileMetaData(gid, is_cleared, all_in_light);

    write_results = false;
    vec2 results = vec2(0.0);
    [[dont_flatten]] if (is_cleared) {
        if (pass != 1 || true) {
            results.x = all_in_light ? 1.0 : 0.0;
            write_results = true;
        }
    } else {
        results = ApplyFilterWithPrecache(did, gtid, stepsize);
        write_results = true;
    }

    return results;
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uvec2 group_thread_id = gl_LocalInvocationID.xy;
    const uvec2 dispatch_thread_id = gl_GlobalInvocationID.xy;

    bool write_output = false;
#if defined(PASS_0)
    const uint PASS_INDEX = 0u;
    const uint STEP_SIZE = 1u;

    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, write_output, PASS_INDEX, STEP_SIZE);
#elif defined(PASS_1)
    const uint PASS_INDEX = 1u;
    const uint STEP_SIZE = 2u;

    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, write_output, PASS_INDEX, STEP_SIZE);
#else
    const uint PASS_INDEX = 2u;
    const uint STEP_SIZE = 4u;

    vec2 results = FilterSoftShadowsPass(group_id, group_thread_id, dispatch_thread_id, write_output, PASS_INDEX, STEP_SIZE);

    // Recover some of the contrast lost during denoising
    const float shadow_remap = max(1.2 - results.y, 1.0);
    const float mean = pow(abs(results.x), shadow_remap);
#endif

    if (write_output) {
#if defined(PASS_0) || defined(PASS_1)
        imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(results, 0.0, 0.0));
#else
        imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(mean, 0.0, 0.0, 0.0));
#endif
    }
}
