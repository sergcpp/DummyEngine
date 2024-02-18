#version 460
#extension GL_EXT_ray_tracing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "ssr_common.glsl"
#include "rt_reflections_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = BIND_UB_SHARED_DATA_BUF, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform sampler2D g_norm_tex;

layout(std430, binding = RAY_LIST_SLOT) readonly buffer RayList {
    uint g_ray_list[];
};

layout(binding = NOISE_TEX_SLOT) uniform lowp sampler2D g_noise_tex;

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
layout(binding = OUT_REFL_IMG_SLOT, r11f_g11f_b10f) uniform writeonly restrict image2D g_out_color_img;
layout(binding = OUT_RAYLEN_IMG_SLOT, r16f) uniform writeonly restrict image2D g_out_raylen_img;

layout(location = 0) rayPayloadEXT RayPayload g_pld;

vec3 SampleReflectionVector(vec3 view_direction, vec3 normal, float roughness, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);
    vec3 view_direction_tbn = tbn_transform * (-view_direction);

    vec2 u = texelFetch(g_noise_tex, ivec2(dispatch_thread_id) % 128, 0).rg;

    vec3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
#ifdef PERFECT_REFLECTIONS
    sampled_normal_tbn = vec3(0.0, 0.0, 1.0); // Overwrite normal sample to produce perfect reflection.
#endif

    vec3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * reflected_direction_tbn);
}

void main() {
    uint packed_coords = g_ray_list[gl_LaunchIDEXT.x];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 icoord = ivec2(ray_coords);
    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    vec4 normal_roughness = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0));
    vec3 normal_ws = normal_roughness.xyz;
    vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

    float roughness = normal_roughness.w;

    const vec2 px_center = vec2(icoord) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 refl_ray_vs = SampleReflectionVector(view_ray_vs, normal_vs, roughness, icoord);
    vec3 refl_ray_ws = (g_shrd_data.inv_view_matrix * vec4(refl_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    g_pld.cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

    { // trace through bvh tree
        const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
        const float t_min = 0.001;
        const float t_max = 1000.0;

        traceRayEXT(g_tlas,               // topLevel
                    ray_flags,          // rayFlags
                    0xff,               // cullMask
                    0,                  // sbtRecordOffset
                    0,                  // sbtRecordStride
                    0,                  // missIndex
                    ray_origin_ws.xyz,  // origin
                    t_min,              // Tmin
                    refl_ray_ws.xyz,    // direction
                    t_max,              // Tmax
                    0                   // payload
                    );
    }

    imageStore(g_out_color_img, icoord, vec4(g_pld.col.rgb, 1.0));
    imageStore(g_out_raylen_img, icoord, vec4(g_pld.cone_width));

    ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(g_pld.cone_width));
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(g_pld.cone_width));
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, 0.0));
        imageStore(g_out_raylen_img, copy_coords, vec4(g_pld.cone_width));
    }
}
