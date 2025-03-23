#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"
#include "blit_down_depth_interface.h"

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out float g_out_color;

void main() {
    ivec2 coord = ivec2(g_vtx_uvs - vec2(0.5));

    float d1 = texelFetch(g_depth_tex, coord + ivec2(0, 0), 0).x;
    float d2 = texelFetch(g_depth_tex, coord + ivec2(0, 1), 0).x;
    float d3 = texelFetch(g_depth_tex, coord + ivec2(1, 1), 0).x;
    float d4 = texelFetch(g_depth_tex, coord + ivec2(1, 0), 0).x;

    //float res_depth = max(max(d1, d2), max(d3, d4));
    float res_depth = min(min(d1, d2), min(d3, d4));
    if (g_params.linearize > 0.5) {
        g_out_color = LinearizeDepth(res_depth, g_params.clip_info);
    } else {
        g_out_color = res_depth;
    }
}
