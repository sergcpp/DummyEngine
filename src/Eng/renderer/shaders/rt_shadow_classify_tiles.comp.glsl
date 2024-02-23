#version 320 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
#extension GL_KHR_shader_subgroup_quad : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "rt_shadow_classify_tiles_interface.h"
#include "rt_shadow_common.glsl.inl"

/*
PERM @NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_shuffle) || !defined(GL_KHR_shader_subgroup_vote))
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = HISTORY_TEX_SLOT) uniform sampler2D g_hist_tex;
layout(binding = PREV_DEPTH_TEX_SLOT) uniform sampler2D g_prev_depth_tex;
layout(binding = PREV_MOMENTS_TEX_SLOT) uniform sampler2D g_prev_moments_tex;

layout(std430, binding = RAY_HITS_BUF_SLOT) readonly buffer RayHits {
    uint g_ray_hits[];
};

layout(std430, binding = OUT_TILE_METADATA_BUF_SLOT) writeonly buffer TileMetadata {
    uint g_tile_metadata[];
};

layout(binding = OUT_REPROJ_RESULTS_IMG_SLOT, rg16f) uniform restrict image2D g_reproj_results_img;
layout(binding = OUT_MOMENTS_IMG_SLOT, r11f_g11f_b10f) uniform restrict image2D g_out_moments_img;

shared int g_false_count;

bool ThreadGroupAllTrue(bool val) {
#ifndef NO_SUBGROUP_EXTENSIONS
    if (gl_SubgroupSize == LOCAL_GROUP_SIZE_X * LOCAL_GROUP_SIZE_Y) {
        return subgroupAll(val);
    } else
#endif
    {
        groupMemoryBarrier(); barrier();
        g_false_count = 0;
        groupMemoryBarrier(); barrier();
        if (!val) {
            g_false_count = 1;
        }
        groupMemoryBarrier(); barrier();
        return g_false_count == 0;
    }
}

bool IsShadowReceiver(uvec2 p) {
    float depth = texelFetch(g_depth_tex, ivec2(p), 0).r;
    return (depth > 0.0) && (depth < 1.0);
}

void WriteTileMetaData(uvec2 gid, uvec2 gtid, bool is_cleared, bool all_in_light) {
    if (all(equal(gtid, uvec2(0)))) {
        uint light_mask = all_in_light ? TILE_META_DATA_LIGHT_MASK : 0u;
        uint clear_mask = is_cleared ? TILE_META_DATA_CLEAR_MASK : 0;
        uint mask = light_mask | clear_mask;

        uint tile_size_x = (g_params.img_size.x + 7) / 8;
        g_tile_metadata[gid.y * tile_size_x + gid.x] = mask;
    }
}

void ClearTargets(uvec2 did, uvec2 gtid, uvec2 gid, float shadow_value, bool is_shadow_receiver, bool all_in_light) {
    WriteTileMetaData(gid, gtid, true, all_in_light);
    imageStore(g_reproj_results_img, ivec2(did), vec4(shadow_value, 0.0, 0.0, 0.0)); // mean, variance

    float temporal_sample_count = is_shadow_receiver ? 1 : 0;
    imageStore(g_out_moments_img, ivec2(did), vec4(shadow_value, 0.0, temporal_sample_count, 0.0)); // mean, variance, temporal sample count
}

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

void SearchSpatialRegion(uvec2 gid, out bool all_in_light, out bool all_in_shadow) {
    // The spatial passes can reach a total region of 1+2+4 = 7x7 around each block.
    // The masks are 8x4, so we need a larger vertical stride

    // Visualization - each x represents a 4x4 block, xx is one entire 8x4 mask as read from the raytracer result
    // Same for yy, these are the ones we are working on right now

    // xx xx xx
    // xx xx xx
    // xx yy xx <-- yy here is the base_tile below
    // xx yy xx
    // xx xx xx
    // xx xx xx

    // All of this should result in scalar ops
    uvec2 base_tile = GetTileIndexFromPixelPosition(gid * ivec2(8, 8));

    // Load the entire region of masks in a scalar fashion
    uint combined_or_mask = 0;
    uint combined_and_mask = 0xFFFFFFFF;
    for (int j = -2; j <= 3; ++j) {
        for (int i = -1; i <= 1; ++i) {
            ivec2 tile_index = ivec2(base_tile) + ivec2(i, j);

            uint ix = (g_params.img_size.x + 7) / 8;
            uint iy = (g_params.img_size.y + 3) / 4;

            tile_index = clamp(tile_index, ivec2(0), ivec2(ix, iy) - 1);
            uint linear_tile_index = LinearTileIndex(tile_index, g_params.img_size.x);
            uint shadow_mask = g_ray_hits[linear_tile_index];

            combined_or_mask = combined_or_mask | shadow_mask;
            combined_and_mask = combined_and_mask & shadow_mask;
        }
    }

    all_in_light = (combined_and_mask == 0xFFFFFFFFu);
    all_in_shadow = (combined_or_mask == 0u);
}

bool IsDisoccluded(uvec2 did, float depth, vec2 velocity) {
    ivec2 dims = ivec2(g_params.img_size);
    vec2 texel_size = g_params.inv_img_size;
    vec2 uv = (vec2(did) + vec2(0.5)) * texel_size;
    vec2 ndc = (2.0 * uv - 1.0) * vec2(1.0, -1.0);
    vec2 previous_uv = uv - velocity;

    bool is_disoccluded = true;
    if (all(greaterThan(previous_uv, vec2(0.0))) && all(lessThan(previous_uv, vec2(1.0)))) {
        // Read the center values
        vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(did), 0).x).xyz;

        vec4 clip_space = g_shrd_data.delta_matrix * vec4(ndc, depth, 1.0);
        clip_space /= clip_space.w; // perspective divide

        // How aligned with the view vector? (the more Z aligned, the higher the depth errors)
        vec4 homogeneous = g_shrd_data.view_from_clip * vec4(ndc, depth, 1.0);
        vec3 world_position = homogeneous.xyz / homogeneous.w;  // perspective divide
        vec3 view_direction = normalize(g_shrd_data.cam_pos_and_gamma.xyz - world_position);
        float z_alignment = 1.0 - dot(view_direction, normal);
        z_alignment = pow(z_alignment, 8);

        // Calculate the depth difference
        float linear_depth = LinearizeDepth(clip_space.z, g_shrd_data.clip_info);   // get linear depth

        ivec2 idx = ivec2(previous_uv * dims);
        float previous_depth = LinearizeDepth(texelFetch(g_prev_depth_tex, idx, 0).r, g_shrd_data.clip_info);
        float depth_difference = abs(previous_depth - linear_depth) / linear_depth;

        // Resolve into the disocclusion mask
        float depth_tolerance = mix(1e-2, 1e-1, z_alignment);
        is_disoccluded = depth_difference >= depth_tolerance;
    }

    return is_disoccluded;
}

vec2 GetClosestVelocity(uvec2 did, float depth) {
    vec2 closest_velocity = texelFetch(g_velocity_tex, ivec2(did), 0).rg;
    float closest_depth = depth;

#ifndef NO_SUBGROUP_EXTENSIONS
    float new_depth = subgroupQuadSwapHorizontal(closest_depth);
    vec2 new_velocity = subgroupQuadSwapHorizontal(closest_velocity);
#else
    // TODO: ...
    float new_depth = 0.0;
    vec2 new_velocity = vec2(0.0);
#endif

#ifdef INVERTED_DEPTH_RANGE
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

#ifndef NO_SUBGROUP_EXTENSIONS
    new_depth = subgroupQuadSwapVertical(closest_depth);
    new_velocity = subgroupQuadSwapVertical(closest_velocity);
#else
    // TODO: ...
#endif

#ifdef INVERTED_DEPTH_RANGE
    if (new_depth > closest_depth)
#else
    if (new_depth < closest_depth)
#endif
    {
        closest_depth = new_depth;
        closest_velocity = new_velocity;
    }

    return closest_velocity;
}

#define KERNEL_RADIUS 8
float KernelWeight(float i) {
#define KERNEL_WEIGHT(i) (exp(-3.0 * float(i * i) / ((KERNEL_RADIUS + 1.0) * (KERNEL_RADIUS + 1.0))))

    // Statically initialize kernel_weights_sum
    float kernel_weights_sum = 0;
    kernel_weights_sum += KERNEL_WEIGHT(0);
    for (int c = 1; c <= KERNEL_RADIUS; ++c) {
        kernel_weights_sum += 2 * KERNEL_WEIGHT(c); // Add other half of the kernel to the sum
    }
    float inv_kernel_weights_sum = rcp(kernel_weights_sum);

    // The only runtime code in this function
    return KERNEL_WEIGHT(i) * inv_kernel_weights_sum;
}

void AccumulateMoments(float value, float weight, inout float moments) {
    // We get value from the horizontal neighborhood calculations. Thus, it's both mean and variance due to using one sample per pixel
    moments += value * weight;
}

// The horizontal part of a 17x17 local neighborhood kernel
float HorizontalNeighborhood(ivec2 did) {
    ivec2 base_did = did;

    // Prevent vertical out of bounds access
    if ((base_did.y < 0) || (base_did.y >= g_params.img_size.y)) return 0;

    uvec2 tile_index = GetTileIndexFromPixelPosition(base_did);
    int linear_tile_index = int(LinearTileIndex(tile_index, g_params.img_size.x));

    int left_tile_index = linear_tile_index - 1;
    int center_tile_index = linear_tile_index;
    int right_tile_index = linear_tile_index + 1;

    bool is_first_tile_in_row = tile_index.x == 0;
    bool is_last_tile_in_row = tile_index.x == (((g_params.img_size.x + 7) / 8) - 1);

    uint left_tile = 0;
    if (!is_first_tile_in_row) left_tile = g_ray_hits[left_tile_index];
    uint center_tile = g_ray_hits[center_tile_index];
    uint right_tile = 0;
    if (!is_last_tile_in_row) right_tile = g_ray_hits[right_tile_index];

    // Construct a single uint with the lowest 17bits containing the horizontal part of the local neighborhood.

    // First extract the 8 bits of our row in each of the neighboring tiles
    uint row_base_index = (did.y % 4) * 8;
    uint left = (left_tile >> row_base_index) & 0xFF;
    uint center = (center_tile >> row_base_index) & 0xFF;
    uint right = (right_tile >> row_base_index) & 0xFF;

    // Combine them into a single mask containting [left, center, right] from least significant to most significant bit
    uint neighborhood = left | (center << 8) | (right << 16);

    // Make sure our pixel is at bit position 9 to get the highest contribution from the filter kernel
    const uint bit_index_in_row = (did.x % 8);
    neighborhood = neighborhood >> bit_index_in_row; // Shift out bits to the right, so the center bit ends up at bit 9.

    float moment = 0.0; // For one sample per pixel this is both, mean and variance

    // First 8 bits up to the center pixel
    uint mask;
    int i;
    for (i = 0; i < 8; ++i) {
        mask = 1u << i;
        moment += (mask & neighborhood) != 0u ? KernelWeight(8 - i) : 0;
    }

    // Center pixel
    mask = 1u << 8;
    moment += (mask & neighborhood) != 0u ? KernelWeight(0) : 0;

    // Last 8 bits
    for (i = 1; i <= 8; ++i) {
        mask = 1u << (8 + i);
        moment += (mask & neighborhood) != 0u ? KernelWeight(i) : 0;
    }

    return moment;
}

shared float g_neighborhood[8][24];

float ComputeLocalNeighborhood(ivec2 did, ivec2 gtid) {
    float local_neighborhood = 0;

    float upper = HorizontalNeighborhood(ivec2(did.x, did.y - 8));
    float center = HorizontalNeighborhood(ivec2(did.x, did.y));
    float lower = HorizontalNeighborhood(ivec2(did.x, did.y + 8));

    g_neighborhood[gtid.x][gtid.y] = upper;
    g_neighborhood[gtid.x][gtid.y + 8] = center;
    g_neighborhood[gtid.x][gtid.y + 16] = lower;

    groupMemoryBarrier(); barrier();

    // First combine the own values.
    // KERNEL_RADIUS pixels up is own upper and KERNEL_RADIUS pixels down is own lower value
    AccumulateMoments(center, KernelWeight(0), local_neighborhood);
    AccumulateMoments(upper, KernelWeight(KERNEL_RADIUS), local_neighborhood);
    AccumulateMoments(lower, KernelWeight(KERNEL_RADIUS), local_neighborhood);

    // Then read the neighboring values.
    for (int i = 1; i < KERNEL_RADIUS; ++i) {
        float upper_value = g_neighborhood[gtid.x][8 + gtid.y - i];
        float lower_value = g_neighborhood[gtid.x][8 + gtid.y + i];
        float weight = KernelWeight(i);
        AccumulateMoments(upper_value, weight, local_neighborhood);
        AccumulateMoments(lower_value, weight, local_neighborhood);
    }

    return local_neighborhood;
}

void TileClassification(uint group_index, uvec2 gid) {
    uvec2 gtid = RemapLane8x8(group_index);
    uvec2 did = gid * 8 + gtid;

    bool is_shadow_receiver = IsShadowReceiver(did);

    bool skip = ThreadGroupAllTrue(!is_shadow_receiver);
    if (skip) {
        ClearTargets(did, gtid, gid, 0, is_shadow_receiver, false);
        return;
    }

    bool all_in_light = false;
    bool all_in_shadow = false;
    SearchSpatialRegion(gid, all_in_light, all_in_shadow);
    float shadow_value = all_in_light ? 1 : 0; // Either all_in_light or all_in_shadow must be true, otherwise we would not skip the tile.

    bool can_skip = all_in_light || all_in_shadow;
    // We have to append the entire tile if there is a single lane that we can't skip
    bool skip_tile = ThreadGroupAllTrue(can_skip);
    if (skip_tile) {
        // We have to set all resources of the tile we skipped to sensible values as neighboring active denoiser tiles might want to read them.
        ClearTargets(did, gtid, gid, shadow_value, is_shadow_receiver, all_in_light);
        return;
    }

    WriteTileMetaData(gid, gtid, false, false);

    float depth = texelFetch(g_depth_tex, ivec2(did), 0).r;
    vec2 velocity = GetClosestVelocity(did.xy, depth); // Must happen before we deactivate lanes

    float local_neighborhood = ComputeLocalNeighborhood(ivec2(did), ivec2(gtid));

    vec2 texel_size = g_params.inv_img_size;
    vec2 uv = (vec2(did.xy) + vec2(0.5)) * texel_size;
    vec2 history_uv = uv - velocity;
    ivec2 history_pos = ivec2(history_uv * g_params.img_size);

    uvec2 tile_index = GetTileIndexFromPixelPosition(did);
    uint linear_tile_index = LinearTileIndex(tile_index, g_params.img_size.x);

    uint shadow_tile = g_ray_hits[linear_tile_index];

    vec3 moments_current = vec3(0.0);
    float variance = 0;
    float shadow_clamped = 0;

    if (is_shadow_receiver) { // do not process sky pixels
        bool hit_light = (shadow_tile & GetBitMaskFromPixelPosition(did)) != 0u;
        float shadow_current = hit_light ? 1.0 : 0.0;

        { // Perform moments and variance calculations
            bool is_disoccluded = IsDisoccluded(did, depth, velocity);
            vec3 previous_moments = is_disoccluded ? vec3(0.0, 0.0, 0.0) // Can't trust previous moments on disocclusion
                                                   //: texelFetch(g_prev_moments_tex, history_pos, 0).xyz;
                                                   : textureLod(g_prev_moments_tex, history_uv, 0.0).xyz;

            float old_m = previous_moments.x;
            float old_s = previous_moments.y;
            float sample_count = previous_moments.z + 1.0;
            float new_m = old_m + (shadow_current - old_m) / sample_count;
            float new_s = old_s + (shadow_current - old_m) * (shadow_current - new_m);

            variance = (sample_count > 1.0 ? new_s / (sample_count - 1.0) : 1.0);
            moments_current = vec3(new_m, new_s, sample_count);
        }

        { // Retrieve local neighborhood and reproject
            float mean = local_neighborhood;
            float spatial_variance = local_neighborhood;

            spatial_variance = max(spatial_variance - mean * mean, 0.0);

            // Compute the clamping bounding box
            const float std_deviation = sqrt(spatial_variance);
            const float nmin = mean - 0.5 * std_deviation;
            const float nmax = mean + 0.5 * std_deviation;

            // Clamp reprojected sample to local neighborhood
            float shadow_previous = shadow_current;
            if (/*FFX_DNSR_Shadows_IsFirstFrame() == 0*/ true) {
                //shadow_previous = FFX_DNSR_Shadows_ReadHistory(history_uv);
                shadow_previous = textureLod(g_hist_tex, history_uv, 0.0).r;
            }

            shadow_clamped = clamp(shadow_previous, nmin, nmax);

            // Reduce history weighting
            const float sigma = 20.0;
            float temporal_discontinuity = (shadow_previous - mean) / max(0.5 * std_deviation, 0.001);
            float sample_counter_damper = exp(-temporal_discontinuity * temporal_discontinuity / sigma);
            moments_current.z *= sample_counter_damper;

            // Boost variance on first frames
            if (moments_current.z < 16.0) {
                float variance_boost = max(16.0 - moments_current.z, 1.0);
                variance = max(variance, spatial_variance);
                variance *= variance_boost;
            }
        }

        // Perform the temporal blend
#if 1 // original code
        float history_weight = sqrt(max(8.0 - moments_current.z, 0.0) / 8.0);
        shadow_clamped = mix(shadow_clamped, shadow_current, mix(0.05, 1.0, history_weight));
#else // linear accumulation
        float accumulation_speed = 1.0 / max(moments_current.z, 1.0);
        float weight = (1.0 - accumulation_speed);
        shadow_clamped = mix(shadow_current, shadow_clamped, weight);
#endif
    }

    // Output the results of the temporal pass
    imageStore(g_reproj_results_img, ivec2(did.xy), vec4(shadow_clamped, variance, 0.0, 0.0));
    imageStore(g_out_moments_img, ivec2(did.xy), vec4(moments_current, 0.0));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    TileClassification(group_index, group_id);
}
