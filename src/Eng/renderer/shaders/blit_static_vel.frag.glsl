#version 430 core

#include "_fs_common.glsl"
#include "blit_static_vel_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_velocity;

void main() {
    ivec2 pix_uvs = ivec2(g_vtx_uvs);

    float depth = texelFetch(g_depth_tex, pix_uvs, 0).x;

    vec4 point_cs = vec4(2.0 * (g_vtx_uvs.xy * g_shrd_data.ren_res.zw) - 1.0, depth, 1.0);
    const vec3 point_vs = TransformFromClipSpace(g_shrd_data.view_from_clip, point_cs);
    const vec3 point_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, point_cs);

    vec4 point_prev_vs = g_shrd_data.prev_view_from_world * vec4(point_ws, 1.0);
    point_prev_vs /= point_prev_vs.w;

    vec4 point_prev_cs = g_shrd_data.prev_clip_from_world * vec4(point_ws, 1.0);
    point_prev_cs /= point_prev_cs.w;

    vec2 unjitter = g_shrd_data.taa_info.xy;
#if defined(VULKAN)
    point_cs.y = -point_cs.y;
#endif
    g_out_velocity.xy = 0.5 * (point_cs.xy + unjitter - point_prev_cs.xy);
#if defined(VULKAN)
    g_out_velocity.y = -g_out_velocity.y;
#endif
    g_out_velocity.xy *= g_shrd_data.ren_res.xy; // improve fp16 utilization
    g_out_velocity.z = -(point_vs.z - point_prev_vs.z);
    g_out_velocity.w = 0.0;
}
