#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_static_vel_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out highp vec2 g_out_velocity;

void main() {
    ivec2 pix_uvs = ivec2(g_vtx_uvs);

    float depth = texelFetch(g_depth_tex, pix_uvs, 0).r;

    vec4 point_cs = vec4(g_vtx_uvs.xy / g_shrd_data.res_and_fres.xy, depth, 1.0);
#if defined(VULKAN)
    point_cs.xy = 2.0 * point_cs.xy - vec2(1.0);
    point_cs.y = -point_cs.y;
#else // VULKAN
    point_cs.xyz = 2.0 * point_cs.xyz - vec3(1.0);
#endif // VULKAN

    vec4 point_ws = g_shrd_data.inv_view_proj_no_translation * point_cs;
    point_ws /= point_ws.w;

    point_ws.xyz += (g_shrd_data.cam_pos_and_gamma.xyz - g_shrd_data.prev_cam_pos.xyz);

    vec4 point_prev_cs = g_shrd_data.prev_view_proj_no_translation * point_ws;
    point_prev_cs /= point_prev_cs.w;

    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif
    g_out_velocity = 0.5 * (point_cs.xy + unjitter - point_prev_cs.xy);
#if defined(VULKAN)
    g_out_velocity.y = -g_out_velocity.y;
#endif
}


