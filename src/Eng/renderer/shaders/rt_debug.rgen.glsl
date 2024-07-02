#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_debug_interface.h"

#if defined(VULKAN)
layout (binding = BIND_UB_SHARED_DATA_BUF, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT g_tlas;
layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D g_out_image;

layout(location = 0) rayPayloadEXT RayPayload g_pld;

void main() {
    const vec2 px_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = in_uv * 2.0 - 1.0;
    d.y = -d.y;

    vec4 origin = g_shrd_data.world_from_view * vec4(0, 0, 0, 1);
    origin /= origin.w;
    vec4 target = g_shrd_data.view_from_clip * vec4(d.xy, 1, 1);
    target /= target.w;
    vec4 direction = g_shrd_data.world_from_view * vec4(normalize(target.xyz), 0);

    const uint ray_flags = 0;//gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.001;
    const float t_max = 1000.0;

    g_pld.cone_width = 0.0;

    traceRayEXT(g_tlas,                     // topLevel
                ray_flags,                  // rayFlags
                (1u << RAY_TYPE_CAMERA),    // cullMask
                0,                          // sbtRecordOffset
                0,                          // sbtRecordStride
                0,                          // missIndex
                origin.xyz,                 // origin
                t_min,                      // Tmin
                direction.xyz,              // direction
                t_max,                      // Tmax
                0                           // payload
                );

    imageStore(g_out_image, ivec2(gl_LaunchIDEXT.xy), vec4(compress_hdr(g_pld.col), 1.0));
}
