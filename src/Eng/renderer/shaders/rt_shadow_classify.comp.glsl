#version 320 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "rt_shadow_classify_interface.h"
#include "rt_shadow_common.glsl.inl"

#pragma multi_compile _ NO_SUBGROUP_EXTENSIONS

#if !defined(NO_SUBGROUP_EXTENSIONS) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_shuffle) || !defined(GL_KHR_shader_subgroup_vote))
#define NO_SUBGROUP_EXTENSIONS
#endif

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
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

layout(binding = SOBOL_BUF_SLOT) uniform highp usamplerBuffer g_sobol_seq_tex;
layout(binding = SCRAMLING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_scrambling_tile_tex;
layout(binding = RANKING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_ranking_tile_tex;

layout(binding = RAY_HITS_IMG_SLOT, r32ui) uniform writeonly uimage2D g_ray_hits_img;
layout(binding = NOISE_IMG_SLOT, rg8) uniform writeonly image2D g_noise_img;

void ClassifyTiles(uvec2 px_coord, uvec2 group_thread_id, uvec2 group_id, bool use_normal, bool use_cascades) {
    bool is_in_viewport = (px_coord.x < g_params.img_size.x && px_coord.y < g_params.img_size.y);

    float depth = texelFetch(g_depth_tex, ivec2(px_coord), 0).r;

    bool is_active_lane = is_in_viewport && (depth < 1.0);

    if (use_normal && is_active_lane) {
        vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(px_coord), 0).x).xyz;

        bool is_facing_sun = dot(normal, g_shrd_data.sun_dir.xyz) > 0.0;
        is_active_lane = is_active_lane && is_facing_sun;
    }

    if (use_cascades && is_active_lane) {
        // TODO: ...
    }

    uint bit_index = LaneIdToBitShift(group_thread_id);
    uint mask = uint(is_active_lane) << bit_index;
#ifndef NO_SUBGROUP_EXTENSIONS
    mask = subgroupOr(mask);

    uint light_mask = ~0u;

    const uint TileTolerance = 1;
    bool discard_tile = subgroupBallotBitCount(uvec4(mask, 0, 0, 0)) <= TileTolerance;
    if (gl_LocalInvocationIndex == 0) {
        if (!discard_tile) {
            uint tile_index = atomicAdd(g_tile_counter[0], 1);
            g_tile_list[tile_index] = PackTile(group_id, mask, 0.0 /* min_t */, 1000.0 /* max_t */);
        }
        imageStore(g_ray_hits_img, ivec2(group_id), uvec4(light_mask));
    }
#else
    // TODO:...
#endif
}

//
// https://eheitzresearch.wordpress.com/762-2/
//
float SampleRandomNumber(in uvec2 pixel, in uint sample_index, in uint sample_dimension) {
    // wrap arguments
    uint pixel_i = pixel.x & 127u;
    uint pixel_j = pixel.y & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    // xor index based on optimized ranking
    uint ranked_sample_index = sample_index ^ texelFetch(g_ranking_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // fetch value in sequence
    uint value = texelFetch(g_sobol_seq_tex, int(sample_dimension + ranked_sample_index * 256u)).r;

    // if the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ texelFetch(g_scrambling_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // convert to float and return
    return (float(value) + 0.5) / 256.0;
}

vec2 SampleRandomVector2D(uvec2 pixel) {
    vec2 u = vec2(fract(SampleRandomNumber(pixel, 0, 6u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO),
                  fract(SampleRandomNumber(pixel, 0, 7u) + float(g_params.frame_index & 0xFFu) * GOLDEN_RATIO));
    return u;
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x < 128u && gl_GlobalInvocationID.y < 128u) {
        imageStore(g_noise_img, ivec2(gl_GlobalInvocationID.xy), vec4(SampleRandomVector2D(gl_GlobalInvocationID.xy), 0, 0));
    }
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    uvec2 group_thread_id = RemapLane8x8(group_index);
    uvec2 dispatch_thread_id = group_id * uvec2(TILE_SIZE_X, TILE_SIZE_Y) + group_thread_id;
    //if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
    //    return;
    //}
    ClassifyTiles(dispatch_thread_id, group_thread_id, group_id, true /* use_normal */, true /* use_cascades */);
}
