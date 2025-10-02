#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "_cs_common.glsl"
#include "bn_pmj_2D_64spp.glsl"
#include "rt_shadow_classify_interface.h"
#include "rt_shadow_common.glsl.inl"

#pragma multi_compile _ NO_SUBGROUP

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;

layout(std430, binding = TILE_COUNTER_SLOT) coherent buffer TileCounter {
    uint g_tile_counter[];
};
layout(std430, binding = TILE_LIST_SLOT) writeonly buffer TileList {
    uvec4 g_tile_list[];
};

layout(binding = BN_PMJ_SEQ_BUF_SLOT) uniform usamplerBuffer g_bn_pmj_seq;

layout(binding = OUT_RAY_HITS_IMG_SLOT, r32ui) uniform restrict writeonly uimage2D g_ray_hits_img;
layout(binding = OUT_NOISE_IMG_SLOT, rg8) uniform restrict writeonly image2D g_noise_img;

shared uint g_shared_mask;

void ClassifyTiles(uvec2 px_coord, uvec2 group_thread_id, uvec2 group_id, bool use_normal, bool use_cascades) {
    bool is_in_viewport = (px_coord.x < g_params.img_size.x && px_coord.y < g_params.img_size.y);

    float depth = texelFetch(g_depth_tex, ivec2(px_coord), 0).x;

    bool is_active_lane = is_in_viewport && (depth < 1.0);

    if (use_normal && is_active_lane) {
        const vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(px_coord), 0).x).xyz;

        const bool is_facing_sun = dot(normal, g_shrd_data.sun_dir.xyz) > 0.0;
        is_active_lane = is_active_lane && is_facing_sun;
    }

    if (use_cascades && is_active_lane) {
        // TODO: ...
    }

    const uint TileTolerance = 1;
    uint light_mask = ~0u;

    const uint bit_index = LaneIdToBitShift(group_thread_id);
    uint mask = uint(is_active_lane) << bit_index;
#ifndef NO_SUBGROUP
    if (gl_NumSubgroups == 1) {
        mask = subgroupOr(mask);

        const bool discard_tile = subgroupBallotBitCount(uvec4(mask, 0, 0, 0)) <= TileTolerance;
        if (gl_LocalInvocationIndex == 0) {
            if (!discard_tile) {
                const uint tile_index = atomicAdd(g_tile_counter[0], 1);
                g_tile_list[tile_index] = PackTile(group_id, mask, 0.0 /* min_t */, 1000.0 /* max_t */);
            }
            imageStore(g_ray_hits_img, ivec2(group_id), uvec4(light_mask));
        }
    } else
#endif
    {
        groupMemoryBarrier(); barrier();
        atomicOr(g_shared_mask, mask);
        groupMemoryBarrier(); barrier();
        mask = g_shared_mask;

        if (gl_LocalInvocationIndex == 0) {
            const bool discard_tile = bitCount(mask) <= TileTolerance;
            if (!discard_tile) {
                const uint tile_index = atomicAdd(g_tile_counter[0], 1);
                g_tile_list[tile_index] = PackTile(group_id, mask, 0.0 /* min_t */, 1000.0 /* max_t */);
            }
            imageStore(g_ray_hits_img, ivec2(group_id), uvec4(light_mask));
        }
    }
}

vec2 SampleRandomVector2D(uvec2 pixel) {
    return Sample2D_BN_PMJ_64SPP(g_bn_pmj_seq, pixel, 4u, g_params.frame_index % 64u);
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_LocalInvocationIndex == 0) {
        g_shared_mask = 0;
    }
    if (gl_GlobalInvocationID.x < 128u && gl_GlobalInvocationID.y < 128u) {
        imageStore(g_noise_img, ivec2(gl_GlobalInvocationID.xy), vec4(SampleRandomVector2D(gl_GlobalInvocationID.xy), 0, 0));
    }
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + group_thread_id;
    //if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
    //    return;
    //}
    ClassifyTiles(dispatch_thread_id, group_thread_id, group_id, true /* use_normal */, true /* use_cascades */);
}
