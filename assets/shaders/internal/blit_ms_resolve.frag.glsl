#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2DMS s_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

void main() {
    outColor = 0.25 * (texelFetch(s_texture, ivec2(aVertexUVs_), 0) +
                       texelFetch(s_texture, ivec2(aVertexUVs_), 1) +
                       texelFetch(s_texture, ivec2(aVertexUVs_), 2) +
                       texelFetch(s_texture, ivec2(aVertexUVs_), 3));
}
