#version 310 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_ballot : enable
#endif

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "ssr_common.glsl"
#include "gi_trace_ss_interface.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    UniformParams : $ubUnifParamLoc
PERM @NO_SUBGROUP_EXTENSIONS
*/

#if !defined(GL_KHR_shader_subgroup_ballot) && !defined(NO_SUBGROUP_EXTENSIONS)
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#define Z_THICKNESS 0.02
#define MAX_STEPS 256
#define MOST_DETAILED_MIP 0
#define LEAST_DETAILED_MIP 6

#define FLOAT_MAX 3.402823466e+38

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_texture;
layout(binding = COLOR_TEX_SLOT) uniform highp sampler2D color_texture;
layout(binding = NORM_TEX_SLOT) uniform highp sampler2D g_norm_texture;
layout(binding = NOISE_TEX_SLOT) uniform lowp sampler2D g_noise_tex;

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

void StoreRay(uint ray_index, uvec2 ray_coord, bool copy_horizontal, bool copy_vertical, bool copy_diagonal) {
    g_out_ray_list[ray_index] = PackRay(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
}

vec3 SampleCosineHemisphere(float u, float v) {
    float phi = 2.0 * M_PI * v;

    float cos_phi = cos(phi);
    float sin_phi = sin(phi);

    float dir = sqrt(u);
    float k = sqrt(1.0 - u);
    return vec3(dir * cos_phi, dir * sin_phi, k);
}

vec3 SampleDiffuseVector(vec3 normal, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);

    vec2 u = texelFetch(g_noise_tex, ivec2(dispatch_thread_id) % 128, 0).rg;

    vec3 direction_tbn = SampleCosineHemisphere(u.x, u.y);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * direction_tbn);
}

//
// https://github.com/GPUOpen-Effects/FidelityFX-SSSR
//
bool IntersectRay(vec3 ray_origin_ss, vec3 ray_origin_vs, vec3 ray_dir_vs, out vec3 out_hit_point) {
    vec4 ray_offsetet_ss = g_shrd_data.proj_matrix * vec4(ray_origin_vs + ray_dir_vs, 1.0);
    ray_offsetet_ss.xyz /= ray_offsetet_ss.w;

#if defined(VULKAN)
    ray_offsetet_ss.y = -ray_offsetet_ss.y;
    ray_offsetet_ss.xy = 0.5 * ray_offsetet_ss.xy + 0.5;
#else // VULKAN
    ray_offsetet_ss.xyz = 0.5 * ray_offsetet_ss.xyz + 0.5;
#endif // VULKAN

    vec3 ray_dir_ss = normalize(ray_offsetet_ss.xyz - ray_origin_ss);
    vec3 ray_dir_ss_inv = mix(1.0 / ray_dir_ss, vec3(FLOAT_MAX), equal(ray_dir_ss, vec3(0.0)));

    int cur_mip = MOST_DETAILED_MIP;

    vec2 cur_mip_res = g_shrd_data.res_and_fres.xy / exp2(MOST_DETAILED_MIP);
    vec2 cur_mip_res_inv = 1.0 / cur_mip_res;

    vec2 uv_offset = 0.005 * exp2(MOST_DETAILED_MIP) / g_shrd_data.res_and_fres.xy;
    uv_offset = mix(uv_offset, -uv_offset, lessThan(ray_dir_ss.xy, vec2(0.0)));

    vec2 floor_offset = mix(vec2(1.0), vec2(0.0), lessThan(ray_dir_ss.xy, vec2(0.0)));

    float cur_t;
    vec3 cur_pos_ss;

    { // advance ray to avoid self intersection
        vec2 cur_mip_pos = cur_mip_res * ray_origin_ss.xy;

        vec2 xy_plane = floor(cur_mip_pos) + floor_offset;
        xy_plane = xy_plane * cur_mip_res_inv + uv_offset;

        vec2 t = (xy_plane - ray_origin_ss.xy) * ray_dir_ss_inv.xy;
        cur_t = min(t.x, t.y);
        cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
    }

    int iter = 0;
    while (iter++ < MAX_STEPS && cur_mip >= MOST_DETAILED_MIP) {
        vec2 cur_pos_px = cur_mip_res * cur_pos_ss.xy;
        float surf_z = texelFetch(g_depth_texture, clamp(ivec2(cur_pos_px), ivec2(0), ivec2(cur_mip_res - 1)), cur_mip).r;
        bool increment_mip = cur_mip < LEAST_DETAILED_MIP;

        { // advance ray
            vec2 xy_plane = floor(cur_pos_px) + floor_offset;
            xy_plane = xy_plane * cur_mip_res_inv + uv_offset;
            vec3 boundary_planes = vec3(xy_plane, surf_z);
            // o + d * t = p' => t = (p' - o) / d
            vec3 t = (boundary_planes - ray_origin_ss.xyz) * ray_dir_ss_inv;

            t.z = (ray_dir_ss.z > 0.0) ? t.z : FLOAT_MAX;

            // choose nearest intersection
            float t_min = min(min(t.x, t.y), t.z);

            bool is_above_surface = surf_z > cur_pos_ss.z;

            increment_mip = increment_mip && (t_min != t.z) && is_above_surface;

            cur_t = is_above_surface ? t_min : cur_t;
            cur_pos_ss = ray_origin_ss.xyz + cur_t * ray_dir_ss;
        }

        cur_mip += increment_mip ? 1 : -1;
        cur_mip_res *= increment_mip ? 0.5 : 2.0;
        cur_mip_res_inv *= increment_mip ? 2.0 : 0.5;
    }

    if (iter > MAX_STEPS) {
        // Intersection was not found
        return false;
    }

    // Reject out-of-view hits
    if (any(lessThan(cur_pos_ss.xy, vec2(0.0))) || any(greaterThan(cur_pos_ss.xy, vec2(1.0)))) {
        return false;
    }

    // Reject if we hit surface from the back
    vec3 hit_normal_ws = UnpackNormalAndRoughness(textureLod(g_norm_texture, cur_pos_ss.xy, 0.0)).xyz;
    vec3 hit_normal_vs = (g_shrd_data.view_matrix * vec4(hit_normal_ws, 0.0)).xyz;
    if (dot(hit_normal_vs, ray_dir_vs) > 0.0) {
        return false;
    }

    vec3 hit_point_cs = cur_pos_ss;
#if defined(VULKAN)
    hit_point_cs.xy = 2.0 * hit_point_cs.xy - 1.0;
    hit_point_cs.y = -hit_point_cs.y;
#else // VULKAN
    hit_point_cs.xyz = 2.0 * hit_point_cs.xyz - 1.0;
#endif // VULKAN

    out_hit_point = hit_point_cs.xyz;

    vec4 hit_point_vs = g_shrd_data.inv_proj_matrix * vec4(hit_point_cs, 1.0);
    hit_point_vs.xyz /= hit_point_vs.w;

    float hit_depth_fetch = texelFetch(g_depth_texture, ivec2(cur_pos_ss.xy * g_params.resolution.xy), 0).r;
    vec4 hit_surf_cs = vec4(hit_point_cs.xy, hit_depth_fetch, 1.0);
#if !defined(VULKAN)
    hit_surf_cs.z = 2.0 * hit_surf_cs.z - 1.0;
#endif // VULKAN

    vec4 hit_surf_vs = g_shrd_data.inv_proj_matrix * hit_surf_cs;
    hit_surf_vs.xyz /= hit_surf_vs.w;
    float dist_vs = distance(hit_point_vs.xyz, hit_surf_vs.xyz);

    return dist_vs < Z_THICKNESS;
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uint ray_index = gl_WorkGroupID.x * 64 + gl_LocalInvocationIndex;
    if (ray_index >= g_inout_ray_counter[1]) return;
    uint packed_coords = g_in_ray_list[ray_index];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 pix_uvs = ivec2(ray_coords);
    if (pix_uvs.x >= g_params.resolution.x || pix_uvs.y >= g_params.resolution.y) return;

    vec2 norm_uvs = (vec2(pix_uvs) + 0.5) / g_shrd_data.res_and_fres.xy;

    vec4 normal_fetch = texelFetch(g_norm_texture, pix_uvs, 0);
    vec3 normal_ws = UnpackNormalAndRoughness(normal_fetch).xyz;
    float depth = texelFetch(g_depth_texture, pix_uvs, 0).r;

    vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

    vec3 ray_origin_ss = vec3(norm_uvs, depth);
    vec4 ray_origin_cs = vec4(ray_origin_ss, 1.0);
#if defined(VULKAN)
    ray_origin_cs.xy = 2.0 * ray_origin_cs.xy - 1.0;
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    ray_origin_cs.xyz = 2.0 * ray_origin_cs.xyz - 1.0;
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = SampleDiffuseVector(normal_vs, pix_uvs);

    vec3 hit_point;
    bool hit_found = IntersectRay(ray_origin_ss, ray_origin_vs.xyz, refl_ray_vs, hit_point);

    // TODO: turn on screen-space tracing!!!
    //hit_found = false;

    vec4 out_color = vec4(0.0, 0.0, 0.0, 100.0);
    if (hit_found) {
        vec2 uv = hit_point.xy;
#if defined(VULKAN)
        uv.y = -uv.y;
#endif // VULKAN
        uv.xy = 0.5 * uv.xy + 0.5;

        out_color = vec4(textureLod(color_texture, uv, 0.0).rgb, distance(hit_point, ray_origin_vs.xyz));
    }

    { // schedule rt rays
        bool needs_ray = !hit_found;
#ifndef NO_SUBGROUP_EXTENSIONS
        uvec4 needs_ray_ballot = subgroupBallot(needs_ray);
        uint local_ray_index_in_wave = subgroupBallotExclusiveBitCount(needs_ray_ballot);
        uint wave_ray_count = subgroupBallotBitCount(needs_ray_ballot);

        uint base_ray_index = 0;
        if (subgroupElect()) {
            base_ray_index = atomicAdd(g_inout_ray_counter[4], wave_ray_count);
        }
        base_ray_index = subgroupBroadcastFirst(base_ray_index);
        if (needs_ray) {
            uint ray_index = base_ray_index + local_ray_index_in_wave;
            StoreRay(ray_index, pix_uvs, copy_horizontal, copy_vertical, copy_diagonal);
        }
#else
        if (needs_ray) {
            uint ray_index = atomicAdd(g_inout_ray_counter[4], 1);
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

