#version 460
#extension GL_EXT_ray_tracing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_rt_common.glsl"
#include "gi_common.glsl"
#include "rt_gi_interface.h"

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
layout(binding = OUT_GI_IMG_SLOT, rgba16f) uniform writeonly restrict image2D g_out_color_img;

layout(location = 0) rayPayloadEXT RayPayload g_pld;

vec3 SampleDiffuseVector(vec3 normal, ivec2 dispatch_thread_id) {
    mat3 tbn_transform = CreateTBN(normal);

    vec2 u = texelFetch(g_noise_tex, ivec2(dispatch_thread_id) % 128, 0).rg;

    vec3 direction_tbn = SampleCosineHemisphere(u.x, u.y);

    // Transform reflected_direction back to the initial space.
    mat3 inv_tbn_transform = transpose(tbn_transform);
    return (inv_tbn_transform * direction_tbn);
}

void main() {
    uint packed_coords = g_ray_list[gl_LaunchIDEXT.x];

    uvec2 ray_coords;
    bool copy_horizontal, copy_vertical, copy_diagonal;
    UnpackRayCoords(packed_coords, ray_coords, copy_horizontal, copy_vertical, copy_diagonal);

    ivec2 icoord = ivec2(ray_coords);
    float depth = texelFetch(g_depth_tex, icoord, 0).r;
    vec3 normal_ws = UnpackNormalAndRoughness(texelFetch(g_norm_tex, icoord, 0)).xyz;
    vec3 normal_vs = normalize((g_shrd_data.view_matrix * vec4(normal_ws, 0.0)).xyz);

    vec2 px_center = vec2(icoord) + 0.5;
    vec2 in_uv = px_center / vec2(g_params.img_size);

#if defined(VULKAN)
    vec4 ray_origin_cs = vec4(2.0 * in_uv - 1.0, depth, 1.0);
    ray_origin_cs.y = -ray_origin_cs.y;
#else // VULKAN
    vec4 ray_origin_cs = vec4(2.0 * vec3(in_uv, depth) - 1.0, 1.0);
#endif // VULKAN

    vec4 ray_origin_vs = g_shrd_data.inv_proj_matrix * ray_origin_cs;
    ray_origin_vs /= ray_origin_vs.w;

    vec3 view_ray_vs = normalize(ray_origin_vs.xyz);
    vec3 gi_ray_vs = SampleDiffuseVector(normal_vs, icoord);
    vec3 gi_ray_ws = (g_shrd_data.inv_view_matrix * vec4(gi_ray_vs.xyz, 0.0)).xyz;

    vec4 ray_origin_ws = g_shrd_data.inv_view_matrix * ray_origin_vs;
    ray_origin_ws /= ray_origin_ws.w;

    // Bias to avoid self-intersection
    // TODO: use flat normal here
    ray_origin_ws.xyz = offset_ray(ray_origin_ws.xyz, normal_ws);

    g_pld.cone_width = g_params.pixel_spread_angle * (-ray_origin_vs.z);

    { // trace through bvh tree
        const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
        const float t_min = 0.001;
        const float t_max = 100.0;

        traceRayEXT(g_tlas,               // topLevel
                    ray_flags,          // rayFlags
                    0xff,               // cullMask
                    0,                  // sbtRecordOffset
                    0,                  // sbtRecordStride
                    0,                  // missIndex
                    ray_origin_ws.xyz,  // origin
                    t_min,              // Tmin
                    gi_ray_ws.xyz,    // direction
                    t_max,              // Tmax
                    0                   // payload
                    );
    }

    imageStore(g_out_color_img, icoord, vec4(g_pld.col.rgb, g_pld.cone_width));

    ivec2 copy_target = icoord ^ 1; // flip last bit to find the mirrored coords along the x and y axis within a quad
    if (copy_horizontal) {
        ivec2 copy_coords = ivec2(copy_target.x, icoord.y);
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, g_pld.cone_width));
    }
    if (copy_vertical) {
        ivec2 copy_coords = ivec2(icoord.x, copy_target.y);
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, g_pld.cone_width));
    }
    if (copy_diagonal) {
        ivec2 copy_coords = copy_target;
        imageStore(g_out_color_img, copy_coords, vec4(g_pld.col.rgb, g_pld.cone_width));
    }
}
