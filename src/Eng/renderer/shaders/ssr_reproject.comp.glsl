#version 320 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "ssr_reproject_interface.h"

#pragma multi_compile _ HQ_HDR

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = DEPTH_HIST_TEX_SLOT) uniform sampler2D g_depth_hist_tex;
layout(binding = NORM_HIST_TEX_SLOT) uniform usampler2D g_norm_hist_tex;
layout(binding = REFL_TEX_SLOT) uniform sampler2D g_refl_tex;
layout(binding = RAYLEN_TEX_SLOT) uniform sampler2D g_raylen_tex;
layout(binding = REFL_HIST_TEX_SLOT) uniform sampler2D g_refl_hist_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = VARIANCE_HIST_TEX_SLOT) uniform sampler2D g_variance_hist_tex;
layout(binding = SAMPLE_COUNT_HIST_TEX_SLOT) uniform sampler2D g_sample_count_hist_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

#ifdef HQ_HDR
    layout(binding = OUT_REPROJECTED_IMG_SLOT, rgba16f) uniform image2D g_out_reprojected_img;
    layout(binding = OUT_AVG_REFL_IMG_SLOT, rgba16f) uniform image2D g_out_avg_refl_img;
#else
    layout(binding = OUT_REPROJECTED_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_reprojected_img;
    layout(binding = OUT_AVG_REFL_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_avg_refl_img;
#endif
layout(binding = OUT_VERIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;
layout(binding = OUT_SAMPLE_COUNT_IMG_SLOT, r16f) uniform image2D g_out_sample_count_img;

bool IsGlossyReflection(float roughness) {
    return roughness <= g_params.thresholds.x;
}

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

/* mediump */ vec4 LoadFromGroupSharedMemoryRaw(ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
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

#define GAUSSIAN_K 3.0

#define LOCAL_NEIGHBORHOOD_RADIUS 4
#define REPROJECTION_NORMAL_SIMILARITY_THRESHOLD 0.9999
#define AVG_RADIANCE_LUMINANCE_WEIGHT 0.3
#define REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT 1.5
#define DISOCCLUSION_NORMAL_WEIGHT 1.4
#define DISOCCLUSION_DEPTH_WEIGHT 1.0
#define DISOCCLUSION_THRESHOLD 0.9
#define SAMPLES_FOR_ROUGHNESS(r) (1.0 - exp(-r * 100.0))

/* mediump */ float GetLuminanceWeight(/* mediump */ vec3 val) {
    /* mediump */ float luma = Luminance(val.xyz);
    /* mediump */ float weight = max(exp(-luma * AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2);
    return weight;
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
            /* mediump */ vec3 radiance = LoadFromGroupSharedMemoryRaw(index).rgb;
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

vec2 GetHitPositionReprojection(ivec2 dispatch_thread_id, vec2 uv, float reflected_ray_length) {
    float z = texelFetch(g_depth_tex, dispatch_thread_id, 0).r;
    vec3 ray_vs = vec3(uv, z);

    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif

    { // project from screen space to view space
        ray_vs.y = (1.0 - ray_vs.y);
        ray_vs.xy = 2.0 * ray_vs.xy - 1.0;
        ray_vs.xy -= unjitter;
        vec4 projected = g_shrd_data.view_from_clip * vec4(ray_vs, 1.0);
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
        // TODO: use matrix without translation here
        vec4 projected = g_shrd_data.world_from_view * vec4(ray_vs, 1.0);
        hit_position_ws = projected.xyz;
    }

    vec2 prev_hit_position;
    { // project to screen space of previous frame
        vec4 projected = g_shrd_data.prev_clip_from_world_no_translation * vec4(hit_position_ws - g_shrd_data.prev_cam_pos.xyz, 1.0);
        projected.xyz /= projected.w;
        projected.xy = 0.5 * projected.xy + 0.5;
        projected.y = (1.0 - projected.y);
        prev_hit_position = projected.xy;
    }
    return prev_hit_position;
}

/* mediump */ float GetDisocclusionFactor(/* mediump */ vec3 normal, /* mediump */ vec3 history_normal, float linear_depth, float history_linear_depth) {
    /* mediump */ float factor = 1.0
                        * exp(-abs(1.0 - max(0.0, dot(normal, history_normal))) * DISOCCLUSION_NORMAL_WEIGHT)
                        * exp(-abs(history_linear_depth - linear_depth) / linear_depth * DISOCCLUSION_DEPTH_WEIGHT);
    return factor;
}

void PickReprojection(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size,
                      /* mediump */ float roughness, /* mediump */ float ray_len,
                      /* mediump */ out float disocclusion_factor, out vec2 reprojection_uv,
                      /* mediump */ out vec3 reprojection) {
    moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);

    vec2 uv = (vec2(dispatch_thread_id) + vec2(0.5)) / vec2(screen_size);
    /* mediump */ vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).xyz;
    /* mediump */ vec3 history_normal;
    float history_linear_depth;

    {
        vec2 motion_vector = texelFetch(g_velocity_tex, ivec2(dispatch_thread_id), 0).xy;
        vec2 surf_repr_uv = uv - motion_vector;
        vec2 hit_repr_uv = GetHitPositionReprojection(ivec2(dispatch_thread_id), uv, ray_len);

        /* mediump */ vec3 surf_history = textureLod(g_refl_hist_tex, surf_repr_uv, 0.0).xyz;
        /* mediump */ vec3 hit_history = textureLod(g_refl_hist_tex, hit_repr_uv, 0.0).xyz;

        /* mediump */ vec4 surf_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, surf_repr_uv, 0.0).x);
        /* mediump */ vec4 hit_fetch = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, hit_repr_uv, 0.0).x);

        /* mediump */ vec3 surf_normal = surf_fetch.xyz, hit_normal = hit_fetch.xyz;
        /* mediump */ float surf_roughness = surf_fetch.w, hit_roughness = hit_fetch.w;

        float hit_normal_similarity = dot(hit_normal, normal);
        float surf_normal_similarity = dot(surf_normal, normal);

        // Choose reprojection uv based on similarity to the local neighborhood
        if (hit_normal_similarity > REPROJECTION_NORMAL_SIMILARITY_THRESHOLD &&
            hit_normal_similarity + 1.0e-3 > surf_normal_similarity &&
            abs(hit_roughness - roughness) < abs(surf_roughness - roughness) + 1.0e-3) {
            // Mirror reflection
            history_normal = hit_normal;
            float hit_history_depth = textureLod(g_depth_hist_tex, hit_repr_uv, 0.0).x;
            history_linear_depth = LinearizeDepth(hit_history_depth, g_shrd_data.clip_info);
            reprojection_uv = hit_repr_uv;
            reprojection = hit_history;
        } else {
            // Reject surface reprojection based on simple distance
            if (length2(surf_history - local_neighborhood.mean) <
                REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT * length(local_neighborhood.variance)) {
                // Surface reflection
                history_normal = surf_normal;
                float surf_history_depth = textureLod(g_depth_hist_tex, surf_repr_uv, 0.0).x;
                history_linear_depth = LinearizeDepth(surf_history_depth, g_shrd_data.clip_info);
                reprojection_uv = surf_repr_uv;
                reprojection = surf_history;
            } else {
                disocclusion_factor = 0.0;
                return;
            }
        }

        float depth = texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).x;
        float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info);
        // Determine disocclusion factor based on history
        disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

        if (disocclusion_factor > DISOCCLUSION_THRESHOLD) {
            return;
        }

        // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            vec2 closest_uv = reprojection_uv;
            vec2 dudv = 1.0 / vec2(screen_size);

            const int SearchRadius = 1;
            for (int y = -SearchRadius; y <= SearchRadius; ++y) {
                for (int x = -SearchRadius; x <= SearchRadius; ++x) {
                    vec2 uv = reprojection_uv + vec2(x, y) * dudv;
                    /* mediump */ vec3 history_normal = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, uv, 0.0).x).xyz;
                    float history_depth = textureLod(g_depth_hist_tex, uv, 0.0).x;
                    float history_linear_depth = LinearizeDepth(history_depth, g_shrd_data.clip_info);
                    /* mediump */ float weight = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
                    if (weight > disocclusion_factor) {
                        disocclusion_factor = weight;
                        closest_uv = uv;
                        reprojection_uv = closest_uv;
                    }
                }
            }
            reprojection = textureLod(g_refl_hist_tex, reprojection_uv, 0.0).rgb;
        }

        // Try to get rid of potential leaks at bilinear interpolation level
        if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
            // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
            // Helps quite a bit on the edges in movement
            float uvx = fract(float(screen_size.x) * reprojection_uv.x + 0.5);
            float uvy = fract(float(screen_size.y) * reprojection_uv.y + 0.5);
            ivec2 reproject_texel_coords = ivec2(vec2(screen_size) * reprojection_uv - vec2(0.5));

            /* mediump */ vec3 reprojection00 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).rgb;
            /* mediump */ vec3 reprojection10 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).rgb;
            /* mediump */ vec3 reprojection01 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).rgb;
            /* mediump */ vec3 reprojection11 = texelFetch(g_refl_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).rgb;

            /* mediump */ vec3 normal00 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).x).xyz;
            /* mediump */ vec3 normal10 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).x).xyz;
            /* mediump */ vec3 normal01 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).x).xyz;
            /* mediump */ vec3 normal11 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).x).xyz;

            float depth00 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 0), 0).x, g_shrd_data.clip_info);
            float depth10 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 0), 0).x, g_shrd_data.clip_info);
            float depth01 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(0, 1), 0).x, g_shrd_data.clip_info);
            float depth11 = LinearizeDepth(texelFetch(g_depth_hist_tex, reproject_texel_coords + ivec2(1, 1), 0).x, g_shrd_data.clip_info);

            /* mediump */ vec4 w;
            // Initialize with occlusion weights
            w.x = GetDisocclusionFactor(normal, normal00, linear_depth, depth00) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.y = GetDisocclusionFactor(normal, normal10, linear_depth, depth10) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.z = GetDisocclusionFactor(normal, normal01, linear_depth, depth01) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            w.w = GetDisocclusionFactor(normal, normal11, linear_depth, depth11) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
            // And then mix in bilinear weights
            w.x = w.x * (1.0 - uvx) * (1.0 - uvy);
            w.y = w.y * (uvx) * (1.0 - uvy);
            w.z = w.z * (1.0 - uvx) * (uvy);
            w.w = w.w * (uvx) * (uvy);
            /* mediump */ float ws = max(w.x + w.y + w.z + w.w, 1.0e-3);
            // normalize
            w /= ws;

            /* mediump */ vec3 history_normal;
            float history_linear_depth;
            reprojection = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
            history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
            history_normal = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;
            disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);
        }
        disocclusion_factor = disocclusion_factor < DISOCCLUSION_THRESHOLD ? 0.0 : disocclusion_factor;
    }
}

void Reproject(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, float temporal_stability_factor, int max_samples) {
    LoadIntoSharedMemory(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    /* mediump */ float variance = 1.0;
    /* mediump */ float sample_count = 0.0;
    /* mediump */ float roughness;
    vec3 normal;
    vec4 fetch = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x);
    normal = fetch.xyz;
    roughness = fetch.w;
    /* mediump */ vec3 refl = texelFetch(g_refl_tex, ivec2(dispatch_thread_id), 0).xyz;
    /* mediump */ float ray_len = texelFetch(g_raylen_tex, ivec2(dispatch_thread_id), 0).x;

    if (IsGlossyReflection(roughness)) {
        /* mediump */ float disocclusion_factor;
        vec2 reprojection_uv;
        /* mediump */ vec3 reprojection;
        PickReprojection(ivec2(dispatch_thread_id), ivec2(group_thread_id), screen_size, roughness, ray_len,
                         disocclusion_factor, reprojection_uv, reprojection);

        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0)))) {
            /* mediump */ float prev_variance = textureLod(g_variance_hist_tex, reprojection_uv, 0.0).x;
            sample_count = textureLod(g_sample_count_hist_tex, reprojection_uv, 0.0).x * disocclusion_factor;
            /* mediump */ float s_max_samples = max(8.0, max_samples * SAMPLES_FOR_ROUGHNESS(roughness));
            sample_count = min(s_max_samples, sample_count + 1);
            /* mediump */ float new_variance = ComputeTemporalVariance(refl, reprojection);
            if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0, 0.0, 0.0, 0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
            } else {
                /* mediump */ float variance_mix = mix(new_variance, prev_variance, 1.0 / sample_count);
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(reprojection, 0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(variance_mix));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(sample_count));
                // Mix in reprojection for radiance mip computation
                refl = mix(refl, reprojection, 0.3);
            }
        } else {
            imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
            imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
            imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
        }
    }

    // Downsample 8x8 -> 1 radiance using shared memory
    // Initialize shared array for downsampling
    /* mediump */ float weight = GetLuminanceWeight(refl);
    refl *= weight;
    if (any(greaterThanEqual(dispatch_thread_id, screen_size)) || any(isinf(refl)) || any(isnan(refl)) || weight > 1.0e3) {
        refl = vec3(0.0);
        weight = 0.0;
    }

    group_thread_id -= 4;

    g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(refl.xy);
    g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(vec2(refl.z, weight));

    groupMemoryBarrier();
    barrier();

    for (int i = 2; i <= 8; i = i * 2) {
        int ox = int(group_thread_id.x) * i;
        int oy = int(group_thread_id.y) * i;
        int ix = int(group_thread_id.x) * i + i / 2;
        int iy = int(group_thread_id.y) * i + i / 2;
        if (ix < 8 && iy < 8) {
            /* mediump */ vec4 rad_weight00 = LoadFromGroupSharedMemoryRaw(ivec2(ox, oy));
            /* mediump */ vec4 rad_weight10 = LoadFromGroupSharedMemoryRaw(ivec2(ox, iy));
            /* mediump */ vec4 rad_weight01 = LoadFromGroupSharedMemoryRaw(ivec2(ix, oy));
            /* mediump */ vec4 rad_weight11 = LoadFromGroupSharedMemoryRaw(ivec2(ix, iy));
            /* mediump */ vec4 sum = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;

            g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.xy);
            g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.zw);
        }
        groupMemoryBarrier();
        barrier();
    }

    if (all(equal(group_thread_id, uvec2(0)))) {
        /* mediump */ vec4 sum = LoadFromGroupSharedMemoryRaw(ivec2(0));
        /* mediump */ float weight_acc = max(sum.w, 1.0e-3);
        vec3 radiance_avg = sum.xyz / weight_acc;

        imageStore(g_out_avg_refl_img, ivec2(dispatch_thread_id / 8), vec4(radiance_avg, 0.0));
    }
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    ivec2  dispatch_group_id = dispatch_thread_id / 8;
    uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, g_params.img_size, 0.7 /* temporal_stability */, 64);
}
