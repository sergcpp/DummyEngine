#version 320 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2DMS g_tex;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

void main() {
    g_out_color = 0.25 * (texelFetch(g_tex, ivec2(g_vtx_uvs), 0) +
                       texelFetch(g_tex, ivec2(g_vtx_uvs), 1) +
                       texelFetch(g_tex, ivec2(g_vtx_uvs), 2) +
                       texelFetch(g_tex, ivec2(g_vtx_uvs), 3));
}
