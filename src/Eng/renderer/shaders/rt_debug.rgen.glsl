#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_debug_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

layout(location = 0) rayPayloadEXT RayPayload g_pld;

void main() {
    const vec2 px_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(gl_LaunchSizeEXT.xy);
    const vec2 d = in_uv * 2.0 - 1.0;

    const vec3 origin = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(d.xy, 1, 1));
    const vec3 target = TransformFromClipSpace(g_shrd_data.world_from_clip, vec4(d.xy, 0, 1));
    const vec3 direction = normalize(target - origin);

    const uint ray_flags = 0;//gl_RayFlagsCullBackFacingTrianglesEXT;

    g_pld.cone_width = 0.0;
    g_pld.throughput = vec3(1.0);
    g_pld.throughput_dist = g_pld.closest_dist = MAX_DIST;

    traceRayEXT(g_tlas,                     // topLevel
                ray_flags,                  // rayFlags
                g_params.cull_mask,         // cullMask
                0,                          // sbtRecordOffset
                0,                          // sbtRecordStride
                0,                          // missIndex
                origin,                     // origin
                0.0,                        // Tmin
                direction,                  // direction
                distance(origin, target),   // Tmax
                0                           // payload
                );

    if (g_pld.closest_dist < g_pld.throughput_dist) {
        g_pld.throughput = vec3(1.0);
    }

    imageStore(g_out_image, ivec2(gl_LaunchIDEXT.xy), vec4(compress_hdr(g_pld.throughput * g_pld.col, g_shrd_data.cam_pos_and_exp.w), 1.0));
}
