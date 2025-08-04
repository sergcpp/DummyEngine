#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_vote : require
#endif

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "bn_pmj_2D_64spp.glsl"
#include "ssr_classify_interface.h"

#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = SPEC_TEX_SLOT) uniform usampler2D g_spec_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
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

layout(binding = BN_PMJ_SEQ_BUF_SLOT) uniform usamplerBuffer g_bn_pmj_seq;

layout(binding = OUT_REFL_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_refl_img;
layout(binding = OUT_NOISE_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_noise_img;

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
shared uint g_shared_bits[2];

void atomic_set_shared_bit(const uvec2 group_thread_id) {
    atomicOr(g_shared_bits[(group_thread_id.y * 8) / 32], (1u << ((group_thread_id.y * 8 + group_thread_id.x) % 32)));
}

bool get_shared_bit(const uvec2 group_thread_id) {
    return (g_shared_bits[(group_thread_id.y * 8) / 32] & (1u << ((group_thread_id.y * 8 + group_thread_id.x) % 32))) != 0;
}

// Taken from https://github.com/GPUOpen-Effects/FidelityFX-Denoiser
void ClassifyTiles(uvec2 dispatch_thread_id, uvec2 group_thread_id, float roughness, uvec2 screen_size, uint samples_per_quad,
                   bool enable_temporal_variance_guided_tracing) {
    if (group_thread_id.x == 0u && group_thread_id.y == 0u) {
        g_tile_count = 0;
        g_shared_bits[0] = g_shared_bits[1] = 0;
    }

#ifndef NO_SUBGROUP
    const bool is_first_lane_of_wave = subgroupElect();
#endif

    bool needs_ray = (dispatch_thread_id.x < screen_size.x && dispatch_thread_id.y < screen_size.y); // disable offscreen pixels

    // Dont shoot a ray on very rough surfaces.
    const bool is_reflective_surface = IsSpecularSurface(g_depth_tex, g_spec_tex, ivec2(dispatch_thread_id));
    const bool is_glossy_reflection = IsGlossyReflection(roughness);
    needs_ray = needs_ray && is_glossy_reflection && is_reflective_surface;

    // Also we dont need to run the denoiser on mirror reflections.
    const bool needs_denoiser = needs_ray && !IsMirrorReflection(roughness);

    // Decide which ray to keep
    bool is_base_ray = IsBaseRay(dispatch_thread_id, samples_per_quad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray); // Make sure to not deactivate mirror reflection rays.

    if (enable_temporal_variance_guided_tracing && needs_denoiser && !needs_ray) {
        const float TemporalVarianceThreshold = 0.01;
        bool has_temporal_variance = texelFetch(g_variance_hist_tex, ivec2(dispatch_thread_id), 0).x > TemporalVarianceThreshold;
        needs_ray = needs_ray || has_temporal_variance;
    }

    groupMemoryBarrier(); barrier();

#ifndef NO_SUBGROUP
    const bool base_needs_ray = subgroupShuffle(needs_ray, gl_SubgroupInvocationID & ~3u);
#else
    // Fallback using shared memory
    if (needs_ray) {
        atomic_set_shared_bit(group_thread_id);
    }
    groupMemoryBarrier(); barrier();

    const bool base_needs_ray = get_shared_bit(group_thread_id & uvec2(~1u));
#endif
    if (!is_base_ray && needs_denoiser && !needs_ray) {
        // If base ray will not be traced, we need to trace this one
        needs_ray = !base_needs_ray;
    }

    // Now we know for each thread if it needs to shoot a ray and wether or not a denoiser pass has to run on this pixel.
    if (is_glossy_reflection && is_reflective_surface) {
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
    // Fallback using shared memory
    groupMemoryBarrier(); barrier();

    if (group_thread_id.x == 0u && group_thread_id.y == 0u) {
        g_shared_bits[0] = g_shared_bits[1] = 0;
    }

    groupMemoryBarrier(); barrier();

    if (require_copy) {
        atomic_set_shared_bit(group_thread_id);
    }

    groupMemoryBarrier(); barrier();

    const bool copy_horizontal = get_shared_bit(group_thread_id ^ uvec2(1u, 0u)) && (samples_per_quad != 4u) && is_base_ray;
    const bool copy_vertical = get_shared_bit(group_thread_id ^ uvec2(0u, 1u)) && (samples_per_quad == 1u) && is_base_ray;
    const bool copy_diagonal = get_shared_bit(group_thread_id ^ uvec2(1u, 1u)) && (samples_per_quad == 1u) && is_base_ray;

    if (needs_ray) {
        const uint ray_index = atomicAdd(g_ray_counter[0], 1);
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }
#endif

    if (g_params.clear > 0.5) {
        imageStore(g_refl_img, ivec2(dispatch_thread_id), vec4(0.0, 0.0, 0.0, -1.0));
    }

    groupMemoryBarrier(); // Wait until all waves write into g_tile_count
    barrier();

    if (group_thread_id.x == 0u && group_thread_id.y == 0u) {
        if (g_tile_count > 0) {
            const uint denoise_tile_index = atomicAdd(g_ray_counter[2], 1);
            g_tile_list[denoise_tile_index] = ((dispatch_thread_id.y & 0xffffu) << 16u) | (dispatch_thread_id.x & 0xffffu);
        } else {
            const uint clear_tile_index = atomicAdd(g_ray_counter[4], 1);
            g_tile_list[g_params.tile_count - clear_tile_index - 1] = ((dispatch_thread_id.y & 0xffffu) << 16u) | (dispatch_thread_id.x & 0xffffu);
        }
    }
}

vec4 SampleRandomVector2D(const uvec2 pixel) {
    return vec4(Sample2D_BN_PMJ_64SPP(g_bn_pmj_seq, pixel, 0u, g_params.frame_index % 64u),
                Sample2D_BN_PMJ_64SPP(g_bn_pmj_seq, pixel, 1u, g_params.frame_index % 64u));
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
    //if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
    //    return;
    //}

    const float roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).w;
    ClassifyTiles(dispatch_thread_id, group_thread_id, roughness, g_params.img_size,
                  g_params.samples_and_guided.x, g_params.samples_and_guided.y != 0u);
}
