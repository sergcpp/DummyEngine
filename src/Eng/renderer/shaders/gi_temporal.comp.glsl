#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#endif

#include "_cs_common.glsl"
#include "gi_common.glsl"
#include "taa_common.glsl"
#include "gi_temporal_interface.h"

#pragma multi_compile _ NO_SUBGROUP

#if !defined(NO_SUBGROUP) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_arithmetic))
#define NO_SUBGROUP
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = AVG_GI_TEX_SLOT) uniform sampler2D g_avg_gi_tex;
layout(binding = FALLBACK_TEX_SLOT) uniform sampler2D g_fallback_tex;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = REPROJ_GI_TEX_SLOT) uniform sampler2D g_reproj_gi_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_GI_IMG_SLOT, rgba16f) uniform image2D g_out_gi_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

#define LOCAL_NEIGHBORHOOD_RADIUS 4

#define GAUSSIAN_K 3.0

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    /* fp16 */ vec4 radiance[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        radiance[i] = texelFetch(g_gi_tex, dispatch_thread_id + offset[i], 0);
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(radiance[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(radiance[i].zw);
    }
}

/* fp16 */ vec4 LoadFromGroupSharedMemory(ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
}

/* fp16 */ float LocalNeighborhoodKernelWeight(/* fp16 */ float i) {
    const /* fp16 */ float radius = LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-GAUSSIAN_K * (i * i) / (radius * radius));
}

struct moments_t {
    /* fp16 */ vec4 mean;
    /* fp16 */ vec3 variance;
};

moments_t EstimateLocalNeighbourhoodInGroup(ivec2 group_thread_id) {
    moments_t ret;
    ret.mean = vec4(0.0);
    ret.variance = vec3(0.0);

    /* fp16 */ float accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            ivec2 index = group_thread_id + ivec2(i, j);
            /* fp16 */ vec4 radiance = LoadFromGroupSharedMemory(index);
            /* fp16 */ float weight = LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
            accumulated_weight += weight;

            ret.mean += radiance * weight;
            ret.variance += radiance.rgb * radiance.rgb * weight;
        }
    }

    ret.mean /= accumulated_weight;
    ret.variance /= accumulated_weight;

    ret.variance = abs(ret.variance - ret.mean.rgb * ret.mean.rgb);

    return ret;
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
void ResolveTemporal(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, vec2 inv_screen_size, float history_clip_weight) {
    LoadIntoSharedMemory(dispatch_thread_id, group_thread_id, ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // Center threads in shared memory

    /* fp16 */ vec4 center_radiance = LoadFromGroupSharedMemory(group_thread_id);
    /* fp16 */ vec4 new_signal = center_radiance;
    /* fp16 */ float new_variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;

    if (center_radiance.w > 0.0) {
        /* fp16 */ float sample_count = texelFetch(g_sample_count_tex, dispatch_thread_id, 0).x;
        const vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        const vec3 avg_radiance = textureLod(g_avg_gi_tex, uv8, 0.0).rgb;
        const vec3 fallback_radiance = texelFetch(g_fallback_tex, dispatch_thread_id, 0).rgb;

        /* fp16 */ vec4 old_signal = texelFetch(g_reproj_gi_tex, dispatch_thread_id, 0);
        moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);
        // Clip history based on the current local statistics
        /* fp16 */ vec3 color_std = (sqrt(local_neighborhood.variance) + length(local_neighborhood.mean.rgb - fallback_radiance)) * history_clip_weight * 1.4;
        local_neighborhood.mean.rgb = mix(local_neighborhood.mean.rgb, avg_radiance, 0.2);
        /* fp16 */ vec3 radiance_min = local_neighborhood.mean.rgb - color_std;
        /* fp16 */ vec3 radiance_max = local_neighborhood.mean.rgb + color_std;
        /* fp16 */ vec4 clipped_old_signal;
        clipped_old_signal.rgb = ClipAABB(radiance_min, radiance_max, old_signal.rgb);
        clipped_old_signal.a = old_signal.a;
        /* fp16 */ float accumulation_speed = 1.0 / max(sample_count, 1.0);
        /* fp16 */ float weight = (1.0 - accumulation_speed);
        // Blend with average for small sample count
        new_signal.rgb = mix(new_signal.rgb, fallback_radiance, 1.0 / max(sample_count + 1.0, 1.0));
        // Clip outliers
        {
            /* fp16 */ vec3 radiance_min = fallback_radiance - color_std * 1.0;
            /* fp16 */ vec3 radiance_max = fallback_radiance + color_std * 1.0;
            new_signal.rgb = ClipAABB(radiance_min, radiance_max, new_signal.rgb);
        }
        // Blend with history
        new_signal = mix(new_signal, clipped_old_signal, weight);
        new_variance = mix(ComputeTemporalVariance(new_signal.rgb, clipped_old_signal.rgb), new_variance, weight);
        if (any(isinf(new_signal)) || any(isnan(new_signal)) || isinf(new_variance) || isnan(new_variance)) {
            new_signal = vec4(0.0);
            new_variance = 0.0;
        }
    }

    imageStore(g_out_gi_img, dispatch_thread_id, new_signal);
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(new_variance));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    const vec2 inv_screen_size = 1.0 / vec2(g_params.img_size);

    ResolveTemporal(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size, inv_screen_size, 0.9 /* history_clip_weight */);
}
