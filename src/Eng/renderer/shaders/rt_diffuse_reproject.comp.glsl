#version 430 core
#extension GL_ARB_shading_language_packing : require

// NOTE: This is not used for now
#if !USE_FP16
    #define float16_t float
    #define f16vec2 vec2
    #define f16vec3 vec3
    #define f16vec4 vec4
#endif

#include "_cs_common.glsl"
#include "rt_diffuse_common.glsl"
#include "taa_common.glsl"
#include "rt_diffuse_reproject_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
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

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_REPROJECTED_IMG_SLOT, rgba16f) uniform image2D g_out_reprojected_img;
layout(binding = OUT_AVG_GI_IMG_SLOT, rgba16f) uniform image2D g_out_avg_gi_img;
layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform image2D g_out_variance_img;
layout(binding = OUT_SAMPLE_COUNT_IMG_SLOT, r8) uniform image2D g_out_sample_count_img;

shared uint g_shared_storage_0[16][16];
shared uint g_shared_storage_1[16][16];

void LoadIntoSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size) {
    // Load 16x16 region into shared memory using 4 8x8 blocks
    const ivec2 offset[4] = {ivec2(0, 0), ivec2(8, 0), ivec2(0, 8), ivec2(8, 8)};

    // Intermediate storage
    f16vec4 refl[4];

    // Start from the upper left corner of 16x16 region
    dispatch_thread_id -= ivec2(4);

    // Load into registers
    for (int i = 0; i < 4; ++i) {
        refl[i] = texelFetch(g_gi_tex, dispatch_thread_id + offset[i], 0);
    }

    // Move to shared memory
    for (int i = 0; i < 4; ++i) {
        const ivec2 index = group_thread_id + offset[i];
        g_shared_storage_0[index.y][index.x] = packHalf2x16(refl[i].xy);
        g_shared_storage_1[index.y][index.x] = packHalf2x16(refl[i].zw);
    }
}

f16vec4 LoadFromSharedMemoryRaw(const ivec2 idx) {
    return vec4(unpackHalf2x16(g_shared_storage_0[idx.y][idx.x]), unpackHalf2x16(g_shared_storage_1[idx.y][idx.x]));
}

#define GAUSSIAN_K 3.0

#define LOCAL_NEIGHBORHOOD_RADIUS 4
#define REPROJECTION_NORMAL_SIMILARITY_THRESHOLD 0.9999
#define AVG_RADIANCE_LUMINANCE_WEIGHT 0.3
#define REPROJECT_SURFACE_DISCARD_VARIANCE_WEIGHT 1.5
#define DISOCCLUSION_NORMAL_WEIGHT 1.4
#define DISOCCLUSION_DEPTH_WEIGHT 0.6
#define DISOCCLUSION_THRESHOLD 0.95

float16_t GetLuminanceWeight(const f16vec3 val) {
    const float16_t luma = Luminance(val.xyz);
    const float16_t weight = max(exp(-luma * AVG_RADIANCE_LUMINANCE_WEIGHT), 1.0e-2);
    return weight;
}

float16_t LocalNeighborhoodKernelWeight(const float16_t i) {
    const float16_t radius = LOCAL_NEIGHBORHOOD_RADIUS + 1.0;
    return exp(-GAUSSIAN_K * (i * i) / (radius * radius));
}

struct moments_t {
    f16vec3 mean;
    f16vec3 variance;
};

moments_t EstimateLocalNeighbourhoodInGroup(ivec2 group_thread_id) {
    moments_t ret;
    ret.mean = f16vec3(0.0);
    ret.variance = f16vec3(0.0);

    float16_t accumulated_weight = 0;
    for (int j = -LOCAL_NEIGHBORHOOD_RADIUS; j <= LOCAL_NEIGHBORHOOD_RADIUS; ++j) {
        for (int i = -LOCAL_NEIGHBORHOOD_RADIUS; i <= LOCAL_NEIGHBORHOOD_RADIUS; ++i) {
            const ivec2 index = group_thread_id + ivec2(i, j);
            const f16vec4 radiance = LoadFromSharedMemoryRaw(index);
            const float16_t weight = float(radiance.w > 0.0) * LocalNeighborhoodKernelWeight(i) * LocalNeighborhoodKernelWeight(j);
            accumulated_weight += weight;

            ret.mean += radiance.xyz * weight;
            ret.variance += radiance.xyz * radiance.xyz * weight;
        }
    }

    ret.mean /= accumulated_weight;
    ret.variance /= accumulated_weight;

    ret.variance = abs(ret.variance - ret.mean * ret.mean);

    return ret;
}

float16_t GetDisocclusionFactor(const float normal_weight_param, const vec2 geometry_weight_params,
                                const f16vec3 center_normal_ws, const f16vec3 history_normal_ws, const f16vec3 center_normal_vs, const f16vec3 history_point_vs) {
    float16_t factor = 1.0;
    factor *= GetEdgeStoppingNormalWeight(normal_weight_param, 0.0, center_normal_ws, history_normal_ws);
    factor *= GetEdgeStoppingPlanarDistanceWeight(geometry_weight_params, center_normal_vs, history_point_vs);
    return factor;
}

void PickReprojection(ivec2 dispatch_thread_id, ivec2 group_thread_id, ivec2 screen_size,
                      out float16_t disocclusion_factor, out vec2 reprojection_uv, out f16vec4 reprojection) {
    const vec2 center_uv = (vec2(dispatch_thread_id) + vec2(0.5)) / vec2(screen_size);
    const f16vec3 center_normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, dispatch_thread_id, 0).x).xyz;
    const f16vec3 center_normal_vs = normalize((g_shrd_data.view_from_world * vec4(center_normal_ws, 0.0)).xyz);

    vec3 motion_vector = texelFetch(g_velocity_tex, dispatch_thread_id, 0).xyz;
    motion_vector.xy *= g_shrd_data.ren_res.zw;
    const vec2 surf_repr_uv = center_uv - motion_vector.xy;

    const f16vec4 surf_history = textureLod(g_gi_hist_tex, surf_repr_uv, 0.0);
    const f16vec3 surf_normal_ws = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, surf_repr_uv, 0.0).x).xyz;
    float history_linear_depth;

    // Surface reflection
    f16vec3 history_normal_ws = surf_normal_ws;
    float surf_history_depth = textureLod(g_depth_hist_tex, surf_repr_uv, 0.0).x;
    history_linear_depth = LinearizeDepth(surf_history_depth, g_shrd_data.clip_info);
    const vec3 history_point_vs = ReconstructViewPosition_YFlip(surf_repr_uv, g_shrd_data.frustum_info, -history_linear_depth, 0.0 /* is_ortho */);
    reprojection_uv = surf_repr_uv;
    reprojection = surf_history;

    const float center_depth = texelFetch(g_depth_tex, dispatch_thread_id, 0).x;
    const float center_depth_lin = LinearizeDepth(center_depth, g_shrd_data.clip_info) - motion_vector.z;
    const vec3 center_point_vs = ReconstructViewPosition_YFlip(center_uv - motion_vector.xy, g_shrd_data.frustum_info, -center_depth_lin, 0.0 /* is_ortho */);

    const float PlaneDistSensitivity = 0.05;
    const vec2 geometry_weight_params = GetGeometryWeightParams(PlaneDistSensitivity, center_point_vs, center_normal_vs, 1.0 /* accumulation_speed */);
    const float normal_weight_param = GetNormalWeightParam(1.0 /* accumulation_speed */, 0.15, 1.0);

    // Determine disocclusion factor based on history
    disocclusion_factor = float(surf_history.w > 0.0);
    disocclusion_factor *= GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, history_normal_ws, center_normal_vs, history_point_vs);

    // Try to find better sample in the vicinity
    if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
        vec2 closest_uv = reprojection_uv;
        const vec2 dudv = 1.0 / vec2(screen_size);

        const int SearchRadius = 1;
        for (int y = -SearchRadius; y <= SearchRadius; ++y) {
            for (int x = -SearchRadius; x <= SearchRadius; ++x) {
                const vec2 uv = reprojection_uv + vec2(x, y) * dudv;
                const f16vec3 history_normal_ws = UnpackNormalAndRoughness(textureLod(g_norm_hist_tex, uv, 0.0).x).xyz;
                const float history_depth = textureLod(g_depth_hist_tex, uv, 0.0).x;
                const float history_linear_depth = LinearizeDepth(history_depth, g_shrd_data.clip_info);
                const vec3 history_point_vs = ReconstructViewPosition_YFlip(uv, g_shrd_data.frustum_info, -history_linear_depth, 0.0 /* is_ortho */);

                float16_t weight = float(textureLod(g_gi_hist_tex, uv, 0.0).w > 0.0);
                weight *= GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, history_normal_ws, center_normal_vs, history_point_vs);
                if (weight > disocclusion_factor) {
                    disocclusion_factor = weight;
                    closest_uv = uv;
                }
            }
        }
        reprojection_uv = closest_uv;
        reprojection = textureLod(g_gi_hist_tex, reprojection_uv, 0.0);
    }

    { // Perform manual bilinear interpolation
        const float uvx = fract(float(screen_size.x) * reprojection_uv.x - 0.5);
        const float uvy = fract(float(screen_size.y) * reprojection_uv.y - 0.5);
        const ivec2 base_pos = ivec2(floor(vec2(screen_size) * reprojection_uv - 0.5));

        const ivec2 sample_pos00 = clamp(base_pos + ivec2(0, 0), ivec2(0), screen_size - 1);
        const ivec2 sample_pos10 = clamp(base_pos + ivec2(1, 0), ivec2(0), screen_size - 1);
        const ivec2 sample_pos01 = clamp(base_pos + ivec2(0, 1), ivec2(0), screen_size - 1);
        const ivec2 sample_pos11 = clamp(base_pos + ivec2(1, 1), ivec2(0), screen_size - 1);

        const f16vec4 reprojection00 = texelFetch(g_gi_hist_tex, sample_pos00, 0);
        const f16vec4 reprojection10 = texelFetch(g_gi_hist_tex, sample_pos10, 0);
        const f16vec4 reprojection01 = texelFetch(g_gi_hist_tex, sample_pos01, 0);
        const f16vec4 reprojection11 = texelFetch(g_gi_hist_tex, sample_pos11, 0);

        const f16vec3 normal00 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, sample_pos00, 0).x).xyz;
        const f16vec3 normal10 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, sample_pos10, 0).x).xyz;
        const f16vec3 normal01 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, sample_pos01, 0).x).xyz;
        const f16vec3 normal11 = UnpackNormalAndRoughness(texelFetch(g_norm_hist_tex, sample_pos11, 0).x).xyz;

        const float depth00 = LinearizeDepth(texelFetch(g_depth_hist_tex, sample_pos00, 0).x, g_shrd_data.clip_info);
        const float depth10 = LinearizeDepth(texelFetch(g_depth_hist_tex, sample_pos10, 0).x, g_shrd_data.clip_info);
        const float depth01 = LinearizeDepth(texelFetch(g_depth_hist_tex, sample_pos01, 0).x, g_shrd_data.clip_info);
        const float depth11 = LinearizeDepth(texelFetch(g_depth_hist_tex, sample_pos11, 0).x, g_shrd_data.clip_info);

        const vec3 point_vs00 = ReconstructViewPosition_YFlip(reprojection_uv, g_shrd_data.frustum_info, -depth00, 0.0 /* is_ortho */);
        const vec3 point_vs10 = ReconstructViewPosition_YFlip(reprojection_uv, g_shrd_data.frustum_info, -depth10, 0.0 /* is_ortho */);
        const vec3 point_vs01 = ReconstructViewPosition_YFlip(reprojection_uv, g_shrd_data.frustum_info, -depth01, 0.0 /* is_ortho */);
        const vec3 point_vs11 = ReconstructViewPosition_YFlip(reprojection_uv, g_shrd_data.frustum_info, -depth11, 0.0 /* is_ortho */);

        f16vec4 w;
        // Occlusion weights
        w.x = float(reprojection00.w > 0.0) * GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, normal00, center_normal_vs, point_vs00) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.y = float(reprojection10.w > 0.0) * GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, normal10, center_normal_vs, point_vs10) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.z = float(reprojection01.w > 0.0) * GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, normal01, center_normal_vs, point_vs01) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        w.w = float(reprojection11.w > 0.0) * GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, normal11, center_normal_vs, point_vs11) > DISOCCLUSION_THRESHOLD / 2.0 ? 1.0 : 0.0;
        // Bilinear weights
        w.x *= (1.0 - uvx) * (1.0 - uvy);
        w.y *= (uvx) * (1.0 - uvy);
        w.z *= (1.0 - uvx) * (uvy);
        w.w *= (uvx) * (uvy);
        // normalize
        w /= max(w.x + w.y + w.z + w.w, 1.0e-3);

        reprojection = reprojection00 * w.x + reprojection10 * w.y + reprojection01 * w.z + reprojection11 * w.w;
        history_linear_depth = depth00 * w.x + depth10 * w.y + depth01 * w.z + depth11 * w.w;
        history_normal_ws = normal00 * w.x + normal10 * w.y + normal01 * w.z + normal11 * w.w;

        const vec3 point_vs = ReconstructViewPosition_YFlip(reprojection_uv, g_shrd_data.frustum_info, -history_linear_depth, 0.0 /* is_ortho */);
        disocclusion_factor = GetDisocclusionFactor(normal_weight_param, geometry_weight_params, center_normal_ws, history_normal_ws, center_normal_vs, point_vs);
    }

    disocclusion_factor = disocclusion_factor < DISOCCLUSION_THRESHOLD ? 0.0 : disocclusion_factor;
}

void Reproject(uvec2 dispatch_thread_id, uvec2 group_thread_id, uvec2 screen_size) {
    LoadIntoSharedMemory(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size));

    groupMemoryBarrier();
    barrier();

    group_thread_id += 4; // center threads in shared memory

    const vec3 normal = UnpackNormalAndRoughness(texelFetch(g_norm_tex, ivec2(dispatch_thread_id), 0).x).xyz;
    f16vec4 gi = texelFetch(g_gi_tex, ivec2(dispatch_thread_id), 0);

    if (gi.w > 0.0) {
        float16_t disocclusion_factor;
        vec2 reprojection_uv;
        f16vec4 reprojection;
        PickReprojection(ivec2(dispatch_thread_id), ivec2(group_thread_id), ivec2(screen_size),
                         disocclusion_factor, reprojection_uv, reprojection);
        reprojection.xyz *= g_params.hist_weight;

        if (all(greaterThan(reprojection_uv, vec2(0.0))) && all(lessThan(reprojection_uv, vec2(1.0)))) {
            float16_t prev_variance = textureLod(g_variance_hist_tex, reprojection_uv, 0.0).x;
            float sample_count = textureLod(g_sample_count_hist_tex, reprojection_uv, 0.0).x * MAX_DIFFUSE_SAMPLES;
            sample_count = min(disocclusion_factor * sample_count + 1, MAX_DIFFUSE_SAMPLES);
            float16_t new_variance = ComputeTemporalVariance(gi.xyz, reprojection.xyz);
            if (disocclusion_factor < DISOCCLUSION_THRESHOLD) {
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0 / MAX_DIFFUSE_SAMPLES));
            } else {
                float16_t variance_mix = mix(new_variance, prev_variance, 1.0 / sample_count);
                imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), reprojection);
                imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(variance_mix));
                imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(sample_count / MAX_DIFFUSE_SAMPLES));
                // Mix in reprojection for radiance mip computation
                //gi = mix(gi, reprojection, 0.3);
            }
        } else {
            imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
            imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(1.0));
            imageStore(g_out_sample_count_img, ivec2(dispatch_thread_id), vec4(1.0 / MAX_DIFFUSE_SAMPLES));
        }
    } else {
        imageStore(g_out_reprojected_img, ivec2(dispatch_thread_id), vec4(0.0));
        imageStore(g_out_variance_img, ivec2(dispatch_thread_id), vec4(0.0));
    }

    // Downsample 8x8 -> 1 radiance using shared memory
    // Initialize shared array for downsampling
    float16_t weight = float16_t(gi.w > 0.0);
    weight *= GetLuminanceWeight(gi.xyz); // this dims down fireflies

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
        const int ox = int(group_thread_id.x) * i;
        const int oy = int(group_thread_id.y) * i;
        const int ix = int(group_thread_id.x) * i + i / 2;
        const int iy = int(group_thread_id.y) * i + i / 2;
        if (ix < 8 && iy < 8) {
            const f16vec4 rad_weight00 = LoadFromSharedMemoryRaw(ivec2(ox, oy));
            const f16vec4 rad_weight10 = LoadFromSharedMemoryRaw(ivec2(ox, iy));
            const f16vec4 rad_weight01 = LoadFromSharedMemoryRaw(ivec2(ix, oy));
            const f16vec4 rad_weight11 = LoadFromSharedMemoryRaw(ivec2(ix, iy));
            const f16vec4 sum = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;

            g_shared_storage_0[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.xy);
            g_shared_storage_1[group_thread_id.y][group_thread_id.x] = packHalf2x16(sum.zw);
        }
        groupMemoryBarrier();
        barrier();
    }

    if (all(equal(group_thread_id, uvec2(0)))) {
        const f16vec4 sum = LoadFromSharedMemoryRaw(ivec2(0));
        const float16_t weight_acc = max(sum.w, 1.0e-3);
        const vec3 radiance_avg = (sum.xyz / weight_acc);

        imageStore(g_out_avg_gi_img, ivec2(dispatch_thread_id / 8), vec4(radiance_avg, 0.0));
    }
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[gl_WorkGroupID.x];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);
    const ivec2  dispatch_group_id = dispatch_thread_id / 8;
    const uvec2 remapped_group_thread_id = RemapLane8x8(gl_LocalInvocationIndex);
    const uvec2 remapped_dispatch_thread_id = dispatch_group_id * 8 + remapped_group_thread_id;

    Reproject(remapped_dispatch_thread_id, remapped_group_thread_id, g_params.img_size);
}
