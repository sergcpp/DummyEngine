#version 310 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_common.glsl"
#include "ssr_common.glsl"
#include "taa_common.glsl"
#include "ssr_resolve_temporal_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
PERM @NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_arithmetic))
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;
layout(binding = AVG_REFL_TEX_SLOT) uniform sampler2D g_avg_refl_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = REPROJ_REFL_TEX_SLOT) uniform sampler2D g_reproj_refl_tex;
layout(binding = VARIANCE_TEX_SLOT) uniform sampler2D g_variance_tex;
layout(binding = SAMPLE_COUNT_TEX_SLOT) uniform sampler2D g_sample_count_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REFL_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_refl_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;

bool IsGlossyReflection(float roughness) {
    return roughness < g_params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < g_params.thresholds.y; //0.0001;
}

#if 0
vec3 EstimateNeighbouhoodDeviation(ivec2 dispatch_thread_id) {
    vec3 color_sum = vec3(0.0);
    vec3 color_sum_squared = vec3(0.0);

    int radius = 1;
    float weight = (radius * 2.0 + 1.0) * (radius * 2.0 + 1.0);

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            ivec2 texel_coords = dispatch_thread_id + ivec2(dx, dy);
            vec3 value = texelFetch(refl_texture, texel_coords, 0).rgb;
            color_sum += value;
            color_sum_squared += value * value;
        }
    }

    vec3 color_std = (color_sum_squared - color_sum * color_sum / weight) / (weight - 1.0);
    return sqrt(max(color_std, 0.0));
}

vec2 GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length) {
    float z = texelFetch(depth_texture, dispatch_thread_id, 0).r;
    vec3 ray_vs = vec3(uv, z);

    vec2 unjitter = shrd_data.uTaaInfo.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif

    { // project from screen space to view space
        ray_vs.y = (1.0 - ray_vs.y);
        ray_vs.xy = 2.0 * ray_vs.xy - 1.0;
        ray_vs.xy -= unjitter;
        vec4 projected = shrd_data.uInvProjMatrix * vec4(ray_vs, 1.0);
        ray_vs = projected.xyz / projected.w;
    }

    // We start out with reconstructing the ray length in view space.
    // This includes the portion from the camera to the reflecting surface as well as the portion from the surface to the hit position.
    float surface_depth = length(ray_vs);
    float ray_length = surface_depth + reflected_ray_length;

    // We then perform a parallax correction by shooting a ray of the same length "straight through" the reflecting surface and reprojecting the tip of that ray to the previous frame.
    ray_vs /= surface_depth; // == normalize(ray_vs)
    ray_vs *= ray_length;
    vec3 hit_position_ws; // This is the "fake" hit position if we would follow the ray straight through the surface.
    { // project from view space to world space
        vec4 projected = shrd_data.uInvViewMatrix * vec4(ray_vs, 1.0);
        hit_position_ws = projected.xyz;
    }

    vec2 prev_hit_position;
    { // project to screen space of previous frame
        vec4 projected = shrd_data.uViewProjPrevMatrix * vec4(hit_position_ws, 1.0);
        projected.xyz /= projected.w;
        projected.xy = 0.5 * projected.xy + 0.5;
        projected.y = (1.0 - projected.y);
        prev_hit_position = projected.xy;
    }
    return prev_hit_position;
}

float SampleHistory(vec2 uv, uvec2 screen_size, vec3 normal, float roughness, vec3 radiance_min, vec3 radiance_max, float roughness_sigma_min, float roughness_sigma_max, float temporal_stability_factor, out vec3 radiance) {
    ivec2 texel_coords = ivec2(screen_size * uv);
    radiance = texelFetch(refl_history, texel_coords, 0).rgb;
    radiance = clip_aabb(radiance_min, radiance_max, radiance);

    vec4 history_normal = UnpackNormalAndRoughness(texelFetch(normal_history, texel_coords, 0));

    const float normal_sigma = 8.0;

    float accumulation_speed = temporal_stability_factor
        * GetEdgeStoppingNormalWeight(normal, history_normal.xyz, normal_sigma)
        * GetEdgeStoppingRoughnessWeight(roughness, history_normal.w, roughness_sigma_min, roughness_sigma_max)
        * GetRoughnessAccumulationWeight(roughness)
        ;

    return saturate(accumulation_speed);
}

float Luma(vec3 col) {
    return max(dot(col, vec3(0.299, 0.587, 0.114)), 0.00001);
}

float ComputeTemporalVariance(vec3 history_radiance, vec3 radiance) {
    // Check temporal variance.
    float history_luminance = Luma(history_radiance);
    float luminance = Luma(radiance);
    return abs(history_luminance - luminance) / max(max(history_luminance, luminance), 0.00001);
}

shared uint g_shared_temp_variance_mask[2];

void WriteTemporalVarianceMask(uint mask_write_index, uint has_temporal_variance_mask) {
    // All lanes write to the same index, so we combine the masks within the wave and do a single write
    uint s_has_temporal_variance_mask = subgroupOr(has_temporal_variance_mask);
    if (subgroupElect()) {
        g_temporal_variance_mask[mask_write_index] = s_has_temporal_variance_mask;
    }
}

uint GetBitMaskFromPixelPosition(uvec2 pixel_pos) {
    uint lane_index = (pixel_pos.y % 4u) * 8u + (pixel_pos.x % 8u);
    return (1u << lane_index);
}

void WriteTemporalVariance(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, bool has_temporal_variance) {
    uint mask_write_index = GetTemporalVarianceIndex(dispatch_thread_id, screen_size.x);
    uint lane_mask = GetBitMaskFromPixelPosition(dispatch_thread_id);
    uint has_temporal_variance_mask = has_temporal_variance ? lane_mask : 0;

#ifndef NO_SUBGROUP_EXTENSIONS
    if (gl_SubgroupSize == 32) {
        WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask);
    } else if (gl_SubgroupSize == 64) { // The lower 32 lanes write to a different index than the upper 32 lanes.
        if (gl_SubgroupInvocationID < 32) {
            WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask); // Write lower
        } else {
            WriteTemporalVarianceMask(mask_write_index, has_temporal_variance_mask); // Write upper
        }
    } else
#endif // NO_SUBGROUP_EXTENSIONS
    { // Use shared memory for all other wave sizes
        uint mask_index = group_thread_id.y / 4;
        g_shared_temp_variance_mask[mask_index] = 0;

        groupMemoryBarrier();
        barrier();

        atomicOr(g_shared_temp_variance_mask[mask_index], has_temporal_variance_mask);

        groupMemoryBarrier();
        barrier();

        if (all(equal(group_thread_id, ivec2(0)))) {
            g_temporal_variance_mask[mask_write_index + 0] = g_shared_temp_variance_mask[0];
            g_temporal_variance_mask[mask_write_index + 1] = g_shared_temp_variance_mask[1];
        }
    }
}

void ResolveTemporal(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float temporal_stability_factor, float temporal_variance_threshold) {
    uint tile_meta_data_index = GetTileMetaDataIndex(dispatch_thread_id, screen_size.x);
    tile_meta_data_index = subgroupBroadcastFirst(tile_meta_data_index);
    bool needs_denoiser = g_tile_metadata_mask[tile_meta_data_index] != 0u;

    bool has_temporal_variance = false;

    if (needs_denoiser) {
        vec4 normal = UnpackNormalAndRoughness(texelFetch(normal_texture, dispatch_thread_id, 0));
        if (!IsGlossyReflection(normal.w) || IsMirrorReflection(normal.w)) {
            return;
        }

        vec2 uv = (vec2(dispatch_thread_id) + 0.5) / vec2(params.img_size);

        vec3 radiance = texelFetch(refl_texture, dispatch_thread_id, 0).rgb;
        vec3 radiance_hist = texelFetch(refl_history, dispatch_thread_id, 0).rgb;
        float ray_len = texelFetch(ray_len_texture, dispatch_thread_id, 0).r;

        vec2 velocity = texelFetch(velocity_texture, dispatch_thread_id, 0).xy;
        vec3 color_deviation = EstimateNeighbouhoodDeviation(dispatch_thread_id);
        color_deviation *= 2.2;

        vec3 radiance_min = radiance - color_deviation;
        vec3 radiance_max = radiance + color_deviation;

        // reproject point on the reflecting surface
        vec2 surf_repr_uv = uv - velocity;

        // reproject hit point
        vec2 hit_repr_uv = GetHitPositionReprojection(dispatch_thread_id, uv, ray_len);

        vec2 repr_uv = (normal.w < 0.05) ? hit_repr_uv : surf_repr_uv;

        vec3 reprojection = vec3(0.0);
        float weight = 0.0;
        if (all(greaterThan(repr_uv, vec2(0.0))) && all(lessThan(repr_uv, vec2(1.0)))) {
            weight = SampleHistory(repr_uv, screen_size, normal.xyz, normal.w, radiance_min, radiance_max, RoughnessSigmaMin, RoughnessSigmaMax, temporal_stability_factor, reprojection);
        }

        radiance = mix(radiance, reprojection, weight);
        has_temporal_variance = ComputeTemporalVariance(radiance_hist, radiance) > temporal_variance_threshold;

        imageStore(out_denoised_img, dispatch_thread_id, vec4(radiance, 1.0));
    }

    WriteTemporalVariance(dispatch_thread_id, group_thread_id, screen_size, has_temporal_variance);
}
#endif

#define LOCAL_NEIGHBORHOOD_RADIUS 4

#define GAUSSIAN_K 3.0

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    /* mediump */ vec3 refl[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        refl[i] = texelFetch(g_refl_tex, dispatch_thread_id + offset[i], 0).xyz;
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(refl[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(refl[i].zz);
    }
}

/* mediump */ vec3 LoadFromGroupSharedMemory(ivec2 idx) {
    return vec3(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]).x);
}

/* mediump */ float LocalNeighborhoodKernelWeight(/* mediump */ float i) {
    const /* mediump */ float radius = LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-GAUSSIAN_K * (i * i) / (radius * radius));
}

struct moments_t {
    /* mediump */ vec3 mean;
    /* mediump */ vec3 variance;
};

moments_t EstimateLocalNeighbourhoodInGroup(ivec2 group_thread_id) {
    moments_t ret;
    ret.mean = vec3(0.0);
    ret.variance = vec3(0.0);

    /* mediump */ float accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            ivec2 index = group_thread_id + ivec2(i, j);
            /* mediump */ vec3 radiance = LoadFromGroupSharedMemory(index);
            /* mediump */ float weight = LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
            accumulated_weight += weight;

            ret.mean += radiance * weight;
            ret.variance += radiance * radiance * weight;
        }
    }

    ret.mean /= accumulated_weight;
    ret.variance /= accumulated_weight;

    ret.variance = abs(ret.variance - ret.mean * ret.mean);

    return ret;
}

// Rounds value to the nearest multiple of 8
uvec2 RoundUp8(uvec2 value) {
    uvec2 round_down = value & ~7u; // 0b111
    return (round_down == value) ? value : value + 8;
}

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
/**********************************************************************
Copyright (c) [2015] [Playdead]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
/* mediump */ vec3 ClipAABB(/* mediump */ vec3 aabb_min, /* mediump */ vec3 aabb_max, /* mediump */ vec3 prev_sample) {
    // Main idea behind clipping - it prevents clustering when neighbor color space
    // is distant from history sample

    // Here we find intersection between color vector and aabb color box

    // Note: only clips towards aabb center
    vec3 aabb_center = 0.5 * (aabb_max + aabb_min);
    vec3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    vec3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    vec3 color_vector_clip = color_vector / extent_clip;
    // Find max absolute component
    color_vector_clip       = abs(color_vector_clip);
    /* mediump */ float max_abs_unit = max(max(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0) {
        return aabb_center + color_vector / max_abs_unit; // clip towards color vector
    } else {
        return prev_sample; // point is inside aabb
    }
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

    /* mediump */ vec3 center_radiance = LoadFromGroupSharedMemory(group_thread_id);
    /* mediump */ vec3 new_signal = center_radiance;
    /* mediump */ float roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0)).w;
    /* mediump */ float new_variance = texelFetch(g_variance_tex, dispatch_thread_id, 0).x;

    if (IsGlossyReflection(roughness)) {
        /* mediump */ float sample_count = texelFetch(g_sample_count_tex, dispatch_thread_id, 0).x;
        vec2 uv8 = (vec2(dispatch_thread_id) + 0.5) / RoundUp8(screen_size);
        /* mediump */ vec3 avg_radiance = textureLod(g_avg_refl_tex, uv8, 0.0).rgb;

        /* mediump */ vec3 old_signal = texelFetch(g_reproj_refl_tex, dispatch_thread_id, 0).rgb;
        moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);
        // Clip history based on the current local statistics
        /* mediump */ vec3 color_std = (sqrt(local_neighborhood.variance) + length(local_neighborhood.mean - avg_radiance)) * history_clip_weight * 1.4;
        local_neighborhood.mean.xyz = mix(local_neighborhood.mean, avg_radiance, 0.2);
        /* mediump */ vec3 radiance_min = local_neighborhood.mean - color_std;
        /* mediump */ vec3 radiance_max = local_neighborhood.mean + color_std;
        /* mediump */ vec3 clipped_old_signal = ClipAABB(radiance_min, radiance_max, old_signal);
        /* mediump */ float accumulation_speed = 1.0 / max(sample_count, 1.0);
        /* mediump */ float weight = (1.0 - accumulation_speed);
        // Blend with average for small sample count
        new_signal = mix(new_signal, avg_radiance, 1.0 / max(sample_count + 1.0f, 1.0));
        // Clip outliers
        {
            /* mediump */ vec3 radiance_min = avg_radiance - color_std * 1.0;
            /* mediump */ vec3 radiance_max = avg_radiance + color_std * 1.0;
            new_signal = ClipAABB(radiance_min, radiance_max, new_signal);
        }
        // Blend with history
        new_signal = mix(new_signal, clipped_old_signal, weight);
        new_variance = mix(ComputeTemporalVariance(new_signal, clipped_old_signal), new_variance, weight);
        if (any(isinf(new_signal)) || any(isnan(new_signal)) || isinf(new_variance) || isnan(new_variance)) {
            new_signal = vec3(0.0);
            new_variance = 0.0;
        }
    }

    imageStore(g_out_refl_img, dispatch_thread_id, vec4(new_signal, 0.0));
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(new_variance));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    ivec2  dispatch_group_id = dispatch_thread_id / 8;
    uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    vec2 inv_screen_size = 1.0 / vec2(g_params.img_size);

    ResolveTemporal(ivec2(remapped_dispatch_thread_id), ivec2(remapped_group_thread_id), g_params.img_size, inv_screen_size, 0.7 /* history_clip_weight */);
}
