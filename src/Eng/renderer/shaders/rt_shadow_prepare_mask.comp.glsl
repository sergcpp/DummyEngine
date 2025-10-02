#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_arithmetic : require
#endif

#include "_cs_common.glsl"
#include "rt_shadow_prepare_mask_interface.h"
#include "rt_shadow_common.glsl.inl"

#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = HIT_MASK_TEX_SLOT) uniform usampler2D g_hit_mask_tex;

layout(std430, binding = SHADOW_MASK_BUF_SLOT) writeonly buffer ShadowMask {
    uint g_shadow_mask[];
};

shared uint g_shared_mask;

void CopyResult(uvec2 gtid, uvec2 gid) {
    uvec2 did = gid * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + gtid;
    uint lin_tile_index = gid.y * ((g_params.img_size.x + TILE_SIZE_X - 1) / TILE_SIZE_X) + gid.x;

    uint mask = texelFetch(g_hit_mask_tex, ivec2(gid), 0).x;
    bool hits_light = ((1u << LaneIdToBitShift(gtid)) & mask) == 0u;

    uint lane_mask = hits_light ? GetBitMaskFromPixelPosition(did) : 0u;
#ifndef NO_SUBGROUP
    if (gl_NumSubgroups == 1) {
        lane_mask = subgroupOr(lane_mask);
    } else
#endif
    {
        groupMemoryBarrier(); barrier();
        atomicOr(g_shared_mask, lane_mask);
        groupMemoryBarrier(); barrier();
        lane_mask = g_shared_mask;
    }

    if (gl_LocalInvocationIndex == 0) {
        g_shared_mask = 0;
        g_shadow_mask[lin_tile_index] = lane_mask;
    }
}

void PrepareShadowMask(uvec2 group_thread_id, uvec2 group_id) {
    group_id *= 4;
    uvec2 tile_dims = (g_params.img_size + uvec2(TILE_SIZE_X - 1, TILE_SIZE_Y - 1)) / uvec2(TILE_SIZE_X, TILE_SIZE_Y);

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            uvec2 tile_id = group_id + uvec2(i, j);
            tile_id = clamp(tile_id, uvec2(0), tile_dims - 1u);
            CopyResult(group_thread_id, tile_id);
        }
    }
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_LocalInvocationIndex == 0) {
        g_shared_mask = 0;
    }
    const uvec2 group_thread_id = gl_LocalInvocationID.xy;
    const uvec2 group_id = gl_WorkGroupID.xy;
    PrepareShadowMask(group_thread_id, group_id);
}
