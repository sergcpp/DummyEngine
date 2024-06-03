#version 3420 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "gi_common.glsl"
#include "gi_reproject_interface.h"

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
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_tex;
layout(binding = GI_HIST_TEX_SLOT) uniform sampler2D g_gi_hist_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = VARIANCE_HIST_TEX_SLOT) uniform sampler2D g_variance_hist_tex;
layout(binding = SAMPLE_COUNT_HIST_TEX_SLOT) uniform sampler2D g_sample_count_hist_tex;
layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exp_tex;

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REPROJECTED_IMG_SLOT, rgba16f) uniform image2D g_out_reprojected_img;
layout(binding = OUT_AVG_GI_IMG_SLOT, rgba16f) uniform image2D g_out_avg_gi_img;
layout(binding = OUT_VERIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;
layout(binding = OUT_SAMPLE_COUNT_IMG_SLOT, r16f) uniform image2D g_out_sample_count_img;

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size, float exposure) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    /* mediump */ vec4 refl[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        refl[i] = texelFetch(g_gi_tex, dispatch_thread_id + offset[i], 0);
        refl[i].xyz *= exposure;
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(refl[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(refl[i].zw);
    }
}

/* mediump */ vec4 LoadFromGroupSharedMemoryRaw(ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]), unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
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
#define DISOCCLUSION_DEPTH_WEIGHT 0.4
#define DISOCCLUSION_THRESHOLD 0.95

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

/* mediump */ float GetDisocclusionFactor(/* mediump */ vec3 normal, /* mediump */ vec3 history_normal, float linear_depth, float history_linear_depth) {
    /* mediump */ float factor = 1.0;
    //factor *= exp(-abs(1.0 - max(0.0, dot(normal, history_normal))) * DISOCCLUSION_NORMAL_WEIGHT);
    factor *= exp(-abs(history_linear_depth - linear_depth) / linear_depth * DISOCCLUSION_DEPTH_WEIGHT);
    return factor;
}

void PickReprojection(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float exposure,
                      /* mediump */ out float disocclusion_factor, out vec2 reprojection_uv,
                      /* mediump */ out vec4 reprojection) {
    moments_t local_neighborhood = EstimateLocalNeighbourhoodInGroup(group_thread_id);

    vec2 uv = (vec2(dispatch_thread_id) + vec2(0.5)) / vec2(screen_size);
    /* mediump */ vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).xyz;
    /* mediump */ vec3 history_normal;

    vec2 motion_vector = texelFetch(g_velocity_tex, ivec2(dispatch_thread_id), 0).xy;
    vec2 surf_repr_uv = uv - motion_vector;

    /* mediump */ vec4 surf_history = textureLod(g_gi_hist_tex, surf_repr_uv, 0.0);
    surf_history.rgb *= exposure;
    /* mediump */ vec3 surf_normal = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, surf_repr_uv, 0.0).x).xyz;
    float history_linear_depth;


    // Surface reflection
    history_normal = surf_normal;
    float surf_history_depth = textureLod(g_depth_hist_tex, surf_repr_uv, 0.0).x;
    history_linear_depth = LinearizeDepth(surf_history_depth, g_shrd_data.clip_info);
    reprojection_uv = surf_repr_uv;
    reprojection = surf_history;


    float depth = texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).x;
    float linear_depth  = LinearizeDepth(depth, g_shrd_data.clip_info);
    // Determine disocclusion factor based on history
    disocclusion_factor = GetDisocclusionFactor(normal, history_normal, linear_depth, history_linear_depth);

    if (disocclusion_factor > DISOCCLUSION_THRESHOLD) {
        return;
    }

    // Try to find the closest sample in the vicinity if we are not convinced of a disocclusion
    if (disocclusion_factor < DISOCCLUSION_THRESHOLD && false) {
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
        reprojection = textureLod(g_gi_hist_tex, reprojection_uv, 0.0);
        reprojection.rgb *= exposure;
    }

    // Try to get rid of potential leaks at bilinear interpolation level
    if (disocclusion_factor < DISOCCLUSION_THRESHOLD && false) {
        // If we've got a discarded history, try to construct a better sample out of 2x2 interpolation neighborhood
        // Helps quite a bit on the edges in movement
        float uvx = fract(float(screen_size.x) * reprojection_uv.x + 0.5);
        float uvy = fract(float(screen_size.y) * reprojection_uv.y + 0.5);
        ivec2 reproject_texel_coords = ivec2(vec2(screen_size) * reprojection_uv - vec2(0.5));

        /* mediump */ vec4 reprojection00 = texelFetch(g_gi_hist_tex, reproject_texel_coords + ivec2(0, 0), 0);
        /* mediump */ vec4 reprojection10 = texelFetch(g_gi_hist_tex, reproject_texel_coords + ivec2(1, 0), 0);
        /* mediump */ vec4 reprojection01 = texelFetch(g_gi_hist_tex, reproject_texel_coords + ivec2(0, 1), 0);
        /* mediump */ vec4 reprojection11 = texelFetch(g_gi_hist_tex, reproject_texel_coords + ivec2(1, 1), 0);

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

void Reproject(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size, int max_samples, float exposure) {
    LoadIntoSharedMemory(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size), exposure);

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    /* mediump */ float variance = 1.0;
    /* mediump */ float sample_count = 0.0;
    vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).xyz;
    /* mediump */ vec4 gi = texelFetch(g_gi_tex, ivec2(dispatch_thread_id), 0);
    gi.xyz *= exposure;

    {
        /* mediump */ float disocclusion_factor;
        vec2 reprojection_uv;
        /* mediump */ vec4 reprojection;
        PickReprojection(ivec2(dispatch_thread_id), ivec2(group_thread_id), screen_size, exposure,
                         disocclusion_factor, reprojection_uv, reprojection);

        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0)))) {
            /* mediump */ float prev_variance = textureLod(g_variance_hist_tex, reprojection_uv, 0.0).x;
            sample_count = textureLod(g_sample_count_hist_tex, reprojection_uv, 0.0).x * disocclusion_factor;
            /* mediump */ float s_max_samples = max_samples;
            sample_count = min(s_max_samples, sample_count + 1);
            /* mediump */ float new_variance = ComputeTemporalVariance(gi.rgb, reprojection.rgb);
            if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
            } else {
                /* mediump */ float variance_mix = mix(new_variance, prev_variance, 1.0 / sample_count);
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(reprojection.xyz / exposure, reprojection.w));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(variance_mix));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(sample_count));
                // Mix in reprojection for radiance mip computation
                //gi = mix(gi, reprojection, 0.3);
            }
        } else {
            imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
            imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
            imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0));
        }
    }

    // Downsample 8x8 -> 1 radiance using shared memory
    // Initialize shared array for downsampling
    /* mediump */ float weight = GetLuminanceWeight(gi.rgb);

    float depth_vs = LinearizeDepth(texelFetch(g_depth_tex, ivec2(dispatch_thread_id), 0).r, g_shrd_data.clip_info);
    if (depth_vs > 100.0) {
        //weight = 0.0;
    }

    gi *= weight;
    if (any(greaterThanEqual(dispatch_thread_id, screen_size)) || any(isinf(gi)) || any(isnan(gi)) || weight > 1.0e3) {
        gi = vec4(0.0);
        weight = 0.0;
    }

    group_thread_id -= 4;

    g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(gi.xy);
    g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(vec2(gi.z, weight));

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
        const vec3 radiance_avg = (sum.xyz / weight_acc);

        imageStore(g_out_avg_gi_img, ivec2(dispatch_thread_id / 8), vec4(radiance_avg / exposure, 0.0));
    }
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    ivec2  dispatch_group_id = dispatch_thread_id / 8;
    uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    const float exposure = texelFetch(g_exp_tex, ivec2(0), 0).x;

    Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, g_params.img_size, 32, exposure);
}
