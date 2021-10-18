#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_down_depth_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2DMS depth_texture;
#else
layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

LAYOUT(location = 0) in highp vec2 aVertexUVs_;

layout(location = 0) out float outColor;

void main() {
    highp ivec2 coord = ivec2(aVertexUVs_ - vec2(0.5));

    highp float d1 = texelFetch(depth_texture, coord + ivec2(0, 0), 0).r;
    highp float d2 = texelFetch(depth_texture, coord + ivec2(0, 1), 0).r;
    highp float d3 = texelFetch(depth_texture, coord + ivec2(1, 1), 0).r;
    highp float d4 = texelFetch(depth_texture, coord + ivec2(1, 0), 0).r;

    //highp float res_depth = max(max(d1, d2), max(d3, d4));
    highp float res_depth = min(min(d1, d2), min(d3, d4));
    if (params.linearize > 0.5) {
        outColor = LinearizeDepth(res_depth, params.clip_info);
    } else {
        outColor = res_depth;
    }
}
