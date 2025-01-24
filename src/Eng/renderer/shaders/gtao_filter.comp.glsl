#version 430 core
#extension GL_EXT_control_flow_attributes : enable

#include "_cs_common.glsl"
#include "gtao_interface.h"

#pragma multi_compile _ HALF_RES

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = GTAO_TEX_SLOT) uniform sampler2D g_gtao_tex;

layout(binding = OUT_IMG_SLOT, r8) uniform image2D g_out_img;

shared float g_shared_ao[16][16];
shared float g_shared_depth[16][16];

struct sample_t {
    float ao;
    float depth;
};

sample_t LoadFromSharedMemory(ivec2 idx) {
    sample_t ret;
    ret.ao = g_shared_ao[idx.y][idx.x];
    ret.depth = g_shared_depth[idx.y][idx.x];
    return ret;
}

void StoreInSharedMemory(const ivec2 idx, const float ao, const float depth) {
    g_shared_ao[idx.y][idx.x] = ao;
    g_shared_depth[idx.y][idx.x] = depth;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 _offset, out float ao, out float depth) {
    dispatch_thread_id += _offset;
    ao = texelFetch(g_gtao_tex, dispatch_thread_id, 0).x;
#ifdef HALF_RES
    depth = LinearizeDepth(texelFetch(g_depth_tex, 2 * dispatch_thread_id, 0).x, g_params.clip_info);
#else
    depth = LinearizeDepth(texelFetch(g_depth_tex, dispatch_thread_id, 0).x, g_params.clip_info);
#endif
}

void StoreWithOffset(ivec2 group_thread_id, const ivec2 _offset, const float ao, const float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, ao, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, const ivec2 group_thread_id) {
    // Load 16x16 region into shared memory.
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    float ao[4];
    float depth[4];

    /// XA
    /// BC

    dispatch_thread_id -= 4; // 1 + 3 => additional band + left band

    for (int i = 0; i < 4; ++i) {
        LoadWithOffset(dispatch_thread_id, offset[i], ao[i], depth[i]);
    }

    for (int i = 0; i < 4; ++i) {
        StoreWithOffset(group_thread_id, offset[i], ao[i], depth[i]);
    }
}

float GetGaussianWeight(float r) {
    // radius is normalized to 1
    return exp( -0.66 * r * r );
}

float GetEdgeStoppingDepthWeight(float center_depth, float neighbor_depth) {
    return exp(-abs(center_depth - neighbor_depth) / center_depth * 32.0);
}

void Filter(ivec2 dispatch_thread_id, ivec2 group_thread_id) {
    InitSharedMemory(dispatch_thread_id, group_thread_id);

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    sample_t center = LoadFromSharedMemory(group_thread_id);

    float accumulated_ao = center.ao;
    float accumulated_weight = 1.0;

    [[unroll]] for (int j = -3; j < 3; ++j) {
        [[unroll]] for (int i = -3; i < 3; ++i) {
            if (i == 0 && j == 0) {
                continue;
            }
            const ivec2 new_idx = group_thread_id + ivec2(i, j);
            const sample_t neighbor = LoadFromSharedMemory(new_idx);

            const float r = sqrt(float(i) * float(i) + float(j) * float(j)) / 3.0;

            float weight = GetGaussianWeight(r);
            weight *= GetEdgeStoppingDepthWeight(center.depth, neighbor.depth);

            accumulated_weight += weight;
            accumulated_ao += weight * neighbor.ao;
        }
    }

    accumulated_ao /= accumulated_weight;

    imageStore(g_out_img, dispatch_thread_id, vec4(accumulated_ao));
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;

    Filter(ivec2(dispatch_thread_id), ivec2(group_thread_id));
}