#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_down_depth_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2DMS g_depth_tex;
#else
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out float g_out_color;

void main() {
    highp ivec2 coord = ivec2(g_vtx_uvs - vec2(0.5));

    highp float d1 = texelFetch(g_depth_tex, coord + ivec2(0, 0), 0).r;
    highp float d2 = texelFetch(g_depth_tex, coord + ivec2(0, 1), 0).r;
    highp float d3 = texelFetch(g_depth_tex, coord + ivec2(1, 1), 0).r;
    highp float d4 = texelFetch(g_depth_tex, coord + ivec2(1, 0), 0).r;

    //highp float res_depth = max(max(d1, d2), max(d3, d4));
    highp float res_depth = min(min(d1, d2), min(d3, d4));
    if (g_params.linearize > 0.5) {
        g_out_color = LinearizeDepth(res_depth, g_params.clip_info);
    } else {
        g_out_color = res_depth;
    }
}
