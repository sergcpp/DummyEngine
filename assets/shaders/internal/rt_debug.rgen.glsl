#version 460
#extension GL_EXT_ray_tracing : require

#include "_common.glsl"
#include "rt_debug_interface.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = TLAS_SLOT) uniform accelerationStructureEXT tlas;
layout(binding = OUT_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_image;

layout(location = 0) rayPayloadEXT RayPayload pld;

void main() {
    const vec2 px_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 in_uv = px_center / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = in_uv * 2.0 - 1.0;
    d.y = -d.y;
    
    vec4 origin = shrd_data.uInvViewMatrix * vec4(0, 0, 0, 1);
    origin /= origin.w;
    vec4 target = shrd_data.uInvProjMatrix * vec4(d.xy, 1, 1);
    target /= target.w;
    vec4 direction = shrd_data.uInvViewMatrix * vec4(normalize(target.xyz), 0);
    
    const uint ray_flags = gl_RayFlagsCullBackFacingTrianglesEXT;
    const float t_min = 0.001;
    const float t_max = 1000.0;
    
    pld.cone_width = 0.0;
    
    traceRayEXT(tlas,           // topLevel
                ray_flags,      // rayFlags
                0xff,           // cullMask
                0,              // sbtRecordOffset
                0,              // sbtRecordStride
                0,              // missIndex
                origin.xyz,     // origin
                t_min,          // Tmin
                direction.xyz,  // direction
                t_max,          // Tmax
                0               // payload
                );
    
    imageStore(out_image, ivec2(gl_LaunchIDEXT.xy), vec4(pld.col, 1.0));
}
