#version 310 es
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_vote : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_common.glsl"
#include "ssr_common.glsl"
#include "ssr_classify_tiles_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

layout(binding = SPEC_TEX_SLOT) uniform sampler2D spec_texture;

layout(std430, binding = TEMP_VARIANCE_MASK_SLOT) readonly buffer TempVarianceMask {
    uint g_temporal_variance_mask[];
};
layout(std430, binding = RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = RAY_LIST_SLOT) writeonly buffer RayList {
    uint g_ray_list[];
};
layout(std430, binding = TILE_METADATA_MASK_SLOT) writeonly buffer TileMetadataMask {
    uint g_tile_metadata_mask[];
};
layout(binding = ROUGH_IMG_SLOT, r8) uniform writeonly image2D roughness_image;

bool IsGlossyReflection(float roughness) {
    return roughness < params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < params.thresholds.y; //0.0001;
}

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

uint LoadTemporalVarianceMask(uint index) {
    return g_temporal_variance_mask[index];
}

uint IncrementRayCounter(uint value) {
    return atomicAdd(g_ray_counter[0], value);
}

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

void StoreTileMetaDataMask(uint mask_index, uint mask) {
    g_tile_metadata_mask[mask_index] = mask;
}

shared uint g_tile_count;

void ClassifyTiles(uvec2 dispatch_thread_id, uvec2 group_thread_id, float roughness, uvec2 screen_size, uint samples_per_quad,
                   bool enable_temporal_variance_guided_tracing) {
    g_tile_count = 0;

    bool is_first_lane_of_wave = subgroupElect();

    bool needs_ray = (dispatch_thread_id.x < screen_size.x && dispatch_thread_id.y < screen_size.y); // disable offscreen pixels

    // Dont shoot a ray on very rough surfaces.
    needs_ray = needs_ray && IsGlossyReflection(roughness);

    // Also we dont need to run the denoiser on mirror reflections.
    bool needs_denoiser = needs_ray && !IsMirrorReflection(roughness);

    // Decide which ray to keep
    bool is_base_ray = IsBaseRay(dispatch_thread_id, samples_per_quad);
    needs_ray = needs_ray && (!needs_denoiser || is_base_ray); // Make sure to not deactivate mirror reflection rays.

    if (enable_temporal_variance_guided_tracing && needs_denoiser && !needs_ray) {
        uint lane_mask = GetBitMaskFromPixelPosition(dispatch_thread_id);
        uint base_mask_index = GetTemporalVarianceIndex(dispatch_thread_id & (~7u), screen_size.x); // 0b111
        base_mask_index = subgroupBroadcastFirst(base_mask_index);

        uint temporal_variance_mask_upper = LoadTemporalVarianceMask(base_mask_index);
        uint temporal_variance_mask_lower = LoadTemporalVarianceMask(base_mask_index + 1);
        uint temporal_variance_mask = group_thread_id.y < 4 ? temporal_variance_mask_upper : temporal_variance_mask_lower;

        bool has_temporal_variance = (temporal_variance_mask & lane_mask) != 0u;
        needs_ray = needs_ray || has_temporal_variance;
    }

    groupMemoryBarrier(); // Wait until g_tile_count is cleared - allow some computations before and after
    barrier();

    // Now we know for each thread if it needs to shoot a ray and wether or not a denoiser pass has to run on this pixel.
    
    // Next we have to figure out which pixels that ray is creating the values for. Thus, if we have to copy its value horizontal, vertical or across.
    bool require_copy = !needs_ray && needs_denoiser; // Our pixel only requires a copy if we want to run a denoiser on it but don't want to shoot a ray for it.
    bool copy_horizontal = (samples_per_quad != 4) && is_base_ray && subgroupShuffle(require_copy, gl_SubgroupInvocationID ^ 1u); // 0b01 QuadReadAcrossX
    bool copy_vertical = (samples_per_quad == 1) && is_base_ray && subgroupShuffle(require_copy, gl_SubgroupInvocationID ^ 2u); // 0b10 QuadReadAcrossY
    bool copy_diagonal = (samples_per_quad == 1) && is_base_ray && subgroupShuffle(require_copy, gl_SubgroupInvocationID ^ 3u); // 0b11 QuadReadAcrossDiagonal

    // Thus, we need to compact the rays and append them all at once to the ray list.
    uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
    uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
    uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);
    
    uint base_ray_index = 0; // leaving this uninitialized causes problems in opengl (???)
    if (is_first_lane_of_wave) {
        base_ray_index = IncrementRayCounter(wave_ray_count);
    }
    base_ray_index = subgroupBroadcastFirst(base_ray_index);
    if (needs_ray) {
        uint ray_index = base_ray_index + local_ray_index_in_wave;
        StoreRay(ray_index, dispatch_thread_id, copy_horizontal, copy_vertical, copy_diagonal);
    }

    // Write tile meta data masks
    bool wave_needs_denoiser = subgroupAny(needs_denoiser);
    if (subgroupElect() && wave_needs_denoiser) {
        atomicAdd(g_tile_count, 1u);
    }

    groupMemoryBarrier(); // Wait until all waves wrote into g_tile_count
    barrier();

    if (group_thread_id.x == 0u && group_thread_id.y == 0u) {
        uint tile_meta_data_index = GetTileMetaDataIndex(subgroupBroadcastFirst(dispatch_thread_id), screen_size.x);
        StoreTileMetaDataMask(tile_meta_data_index, g_tile_count);
    }
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= params.img_size.x || gl_GlobalInvocationID.y >= params.img_size.y) {
        return;
    }
    
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    uvec2 group_thread_id = RemapLane8x8(group_index);
    uvec2 dispatch_thread_id = group_id * 8 + group_thread_id;
    
    float roughness = texelFetch(spec_texture, ivec2(dispatch_thread_id), 0).w;
    
    ClassifyTiles(dispatch_thread_id, group_thread_id, roughness, params.img_size, params.samples_and_guided.x, params.samples_and_guided.y != 0u);
    
    imageStore(roughness_image, ivec2(dispatch_thread_id), vec4(roughness));
}
