#version 320 es
#ifndef NO_SUBGROUP
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
#include "gi_common.glsl"
#include "gi_classify_interface.h"

#pragma multi_compile _ NO_SUBGROUP

#if !defined(NO_SUBGROUP) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_shuffle) || !defined(GL_KHR_shader_subgroup_vote))
#define NO_SUBGROUP
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_spec_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_hist_tex;

layout(std430, binding = RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = RAY_LIST_SLOT) writeonly buffer RayList {
    uint g_ray_list[];
};
layout(std430, binding = TILE_LIST_SLOT) writeonly buffer TileList {
    uint g_tile_list[];
};
layout(binding = SOBOL_BUF_SLOT) uniform highp usamplerBuffer g_sobol_seq_tex;
layout(binding = SCRAMLING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_scrambling_tile_tex;
layout(binding = RANKING_TILE_BUF_SLOT) uniform highp usamplerBuffer g_ranking_tile_tex;

layout(binding = GI_IMG_SLOT, rgba16f) uniform writeonly image2D g_gi_img;
layout(binding = NOISE_IMG_SLOT, rgba8) uniform writeonly image2D g_noise_img;

bool IsBaseRay(uvec2 dispatch_thread_id, uint samples_per_quad) {
    switch (samples_per_quad) {
    case 1:
        return ((dispatch_thread_id.x & 1u) | (dispatch_thread_id.y & 1u)) == 0u; // Deactivates 3 out of 4 rays
    case 2:
        return (dispatch_thread_id.x & 1u) == (dispatch_thread_id.y & 1u); // Deactivates 2 out of 4 rays. Keeps diagonal.
    default: // case 4:
        return true;
    }
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4) * 8 + (pixel_pos.x % 8);
    return (1u << lane_index);
}

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

shared uint g_tile_count;

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
void ClassifyTiles(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, uint samples_per_quad,
                   bool enable_temporal_variance_guided_tracing) {
    g_tile_count = 0;

#ifndef NO_SUBGROUP
    const bool is_first_lane_of_wave = subgroupElect();
#endif

    bool needs_ray = (dispatch_thread_id.x < screen_size.x && dispatch_thread_id.y < screen_size.y); // disable offscreen pixels

    // Dont shoot a ray on metal surfaces
    const bool is_diffuse_surface = IsDiffuseSurface(g_depth_tex, g_spec_tex, ivec2(dispatch_thread_id));
    needs_ray = needs_ray && is_diffuse_surface;

    //
    bool needs_denoiser = needs_ray;

    // Decide which ray to keep
    const bool is_base_ray = IsBaseRay(dispatch_thread_id, samples_per_quad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray);

    if (enable_temporal_variance_guided_tracing && needs_denoiser && !needs_ray) {
        const float TemporalVarianceThreshold = 0.025;
        needs_ray = (texelFetch(g_variance_hist_tex, ivec2(dispatch_thread_id), 0).r > TemporalVarianceThreshold);
    }

#ifndef NO_SUBGROUP
    const bool base_needs_ray = subgroupShuffle(needs_ray, gl_SubgroupInvocationID & ~3u);
    if (!is_base_ray && needs_denoiser && !needs_ray) {
        // If base ray will not be traced, we need to trace this one
        needs_ray = !base_needs_ray;
    }
#else
    // TODO: Fallback using shared memory
#endif

    groupMemoryBarrier(); // Wait until g_tile_count is cleared - allow some computations before and after
    barrier();

    // Now we know for each thread if it needs to shoot a ray and wether or not a denoiser pass has to run on this pixel.

    if (is_diffuse_surface) {
        atomicAdd(g_tile_count, 1u);
    }

    // Next we have to figure out which pixels that ray is creating the values for. Thus, if we have to copy its value horizontal, vertical or across.
    const bool require_copy = !needs_ray && needs_denoiser; // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
#ifndef NO_SUBGROUP
     // Subgroup reads need to be unconditional (should be first), probably a compiler bug!!!
    const bool copy_horizontal = subgroupShuffleXor(require_copy, 1u) && (samples_per_quad != 4u) && is_base_ray; // 0b01 QuadReadAcrossX
    const bool copy_vertical = subgroupShuffleXor(require_copy, 2u) && (samples_per_quad == 1u) && is_base_ray; // 0b10 QuadReadAcrossY
    const bool copy_diagonal = subgroupShuffleXor(require_copy, 3u) && (samples_per_quad == 1u) && is_base_ray; // 0b11 QuadReadAcrossDiagonal

    // Thus, we need to compact the rays and append them all at once to the ray list.
    const uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
    const uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
    const uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

    uint base_ray_index = 0; // leaving this uninitialized causes problems in opengl (???)
    if (is_first_lane_of_wave) {
        // increment ray counter
        base_ray_index = atomicAdd(g_ray_counter[0], wave_ray_count);
    }
    base_ray_index = subgroupBroadcastFirst(base_ray_index);
    if (needs_ray) {
        const uint ray_index = base_ray_index + local_ray_index_in_wave;
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#else
    // TODO: Fallback using shared memory
    const bool copy_horizontal = /*[group_thread_id.y][group_thread_id.x ^ 1u] &&*/ (samples_per_quad != 4u) && is_base_ray;
    const bool copy_vertical = /*subgroupShuffleXor(require_copy, 2u) &&*/ (samples_per_quad == 1u) && is_base_ray;
    const bool copy_diagonal = /*subgroupShuffleXor(require_copy, 3u) &&*/ (samples_per_quad == 1u) && is_base_ray;

    if (needs_ray) {
        const uint ray_index = atomicAdd(g_ray_counter[0], 1);
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#endif

    imageStore(g_gi_img, ivec2(dispatch_thread_id), vec4(0.0, 0.0, 0.0, -1.0));

    groupMemoryBarrier(); // Wait until all waves write into g_tile_count
    barrier();

    if (group_thread_id.x == 0u && group_thread_id.y == 0u && g_tile_count > 0) {
        uint tile_index = atomicAdd(g_ray_counter[2], 1);
        g_tile_list[tile_index] = ((dispatch_thread_id.y & 0xffffu) << 16u) | (dispatch_thread_id.x & 0xffffu);
    }
}

//
// https://eheitzresearch.wordpress.com/762-2/
//
float SampleRandomNumber(in uvec2 pixel, in uint sample_index, in uint sample_dimension) {
    // wrap arguments
    const uint pixel_i = pixel.x & 127u;
    const uint pixel_j = pixel.y & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

    // xor index based on optimized ranking
    const uint ranked_sample_index = sample_index ^ texelFetch(g_ranking_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // fetch value in sequence
    uint value = texelFetch(g_sobol_seq_tex, int(sample_dimension + ranked_sample_index * 256u)).r;

    // if the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ texelFetch(g_scrambling_tile_tex, int((sample_dimension & 7u) + (pixel_i + pixel_j * 128u) * 8u)).r;

    // convert to float and return
    return (float(value) + 0.5) / 256.0;
}

vec4 SampleRandomVector2D(const uvec2 pixel) {
    return vec4(SampleRandomNumber(pixel, g_params.frame_index % 32u, 4u),
                SampleRandomNumber(pixel, g_params.frame_index % 32u, 5u),
                SampleRandomNumber(pixel, g_params.frame_index % 32u, 6u),
                SampleRandomNumber(pixel, g_params.frame_index % 32u, 7u));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x < 128u && gl_GlobalInvocationID.y < 128u) {
        imageStore(g_noise_img, ivec2(gl_GlobalInvocationID.xy), SampleRandomVector2D(gl_GlobalInvocationID.xy));
    }
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;

    ClassifyTiles(dispatch_thread_id, group_thread_id, g_params.img_size,
                  g_params.samples_and_guided.x, g_params.samples_and_guided.y != 0u);
}
