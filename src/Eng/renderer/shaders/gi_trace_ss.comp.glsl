#version 430 core
#ifndef NO_SUBGROUP
#extension GL_KHR_shader_subgroup_ballot : enable
#endif

#include "_cs_common.glsl"
#include "rt_common.glsl"
#include "gi_common.glsl"
#include "principled_common.glsl"

#include "gi_trace_ss_interface.h"

#pragma multi_compile _ NO_SUBGROUP

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = COLOR_TEX_SLOT) uniform sampler2D color_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_norm_tex;
layout(binding = NOISE_TEX_SLOT) uniform sampler2D g_noise_tex;

layout(std430, binding = IN_RAY_LIST_SLOT) readonly buffer InRayList {
    uint g_in_ray_list[];
};

layout(binding = OUT_GI_IMG_SLOT, rgba16f) uniform image2D g_out_color_img;
layout(std430, binding = OUT_RAY_LIST_SLOT) writeonly buffer OutRayList {
    uint g_out_ray_list[];
};
layout(std430, binding = INOUT_RAY_COUNTER_SLOT) coherent buffer RayCounter {
    uint g_inout_ray_counter[];
};

#include "ss_trace_hierarchical.glsl.inl"

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_out_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

vec3 SampleDiffuseVector(vec3 normal, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);

    vec2 u = texelFetch(g_noise_tex, ivec2(dispatch_thread_id) % 128, 0).xy;

    vec3 direction_tbn = SampleCosineHemisphere(u.x, u.y);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * direction_tbn);
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint ray_index = gl_WorkGroupID.x * LOCAL_GROUP_SIZE_X + gl_LocalInvocationIndex;
    if (ray_index >= g_inout_ray_counter[1]) return;
    uint packed_coords = g_in_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 pix_uvs = ivec2(ray_coords);
    if (pix_uvs.x >= g_params.resolution.x || pix_uvs.y >= g_params.resolution.y) return;
    vec2 norm_uvs = (vec2(pix_uvs) + 0.5) / g_shrd_data.res_and_fres.xy;

    vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, pix_uvs, 0).x).xyz;
    float depth = texelFetch(g_depth_tex, pix_uvs, 0).x;

    vec3 normal_vs = normalize((g_shrd_data.view_from_world * vec4(normal_ws, 0.0)).xyz);

    const vec3 ray_origin_ss = vec3(norm_uvs, depth);
    const vec4 ray_origin_cs = vec4(2.0 * ray_origin_ss.xy - 1.0, ray_origin_ss.z, 1.0);
    const vec3 ray_origin_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, ray_origin_cs);

    const vec3 view_ray_vs = normalize(ray_origin_vs);
    const vec3 refl_ray_vs = SampleDiffuseVector(normal_vs, pix_uvs);

    vec3 hit_point_cs, hit_point_vs, hit_normal_vs;
    bool hit_found = IntersectRay(ray_origin_ss, ray_origin_vs, refl_ray_vs, g_depth_tex, g_norm_tex, hit_point_cs, hit_point_vs, hit_normal_vs);

    vec4 out_color = vec4(0.0, 0.0, 0.0, 100.0);
    if (hit_found) {
        vec2 uv = hit_point_cs.xy;
#if defined(VULKAN)
        uv.y = -uv.y;
#endif // VULKAN
        uv.xy = 0.5 * uv.xy + 0.5;

        const float hit_t = distance(hit_point_vs, ray_origin_vs);
        out_color = textureLod(color_tex, uv, 0.0);

        const vec4 is_emissive = textureGather(color_tex, uv, 3);
        if (any(greaterThanEqual(is_emissive, vec4(1.0)))) {
            // Skip emissive surface
            hit_found = false;
        }
        out_color.w = hit_t;
    }

    out_color.w = GetNormHitDist(out_color.w, -ray_origin_vs.z, 1.0);

    { // schedule rt rays
        bool needs_ray = !hit_found;
#ifndef NO_SUBGROUP
        uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
        uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
        uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

        uint base_ray_index = 0;
        if (subgroupElect()) {
            base_ray_index = atomicAdd(g_inout_ray_counter[6], wave_ray_count);
        }
        base_ray_index = subgroupBroadcastFirst(base_ray_index);
        if (needs_ray) {
            uint ray_index = base_ray_index + local_ray_index_in_wave;
            StoreRay(ray_index, pix_uvs, copy_horizontal, copy_vertical, copy_diagonal);
        }
#else
        if (needs_ray) {
            uint ray_index = atomicAdd(g_inout_ray_counter[6], 1);
            StoreRay(ray_index, pix_uvs, copy_horizontal, copy_vertical, copy_diagonal);
        }
#endif
    }

    imageStore(g_out_color_img, pix_uvs, out_color);

    ivec2 copy_target = pix_uvs ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, pix_uvs.y);
        imageStore(g_out_color_img, copy_coords, out_color);
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(pix_uvs.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, out_color);
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, out_color);
    }
}

