#version 320 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssr_dilate_interface.h"

layout(binding = SSR_TEX_SLOT) uniform highp sampler2D g_source_tex;

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec3 g_out_color;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

    g_out_color = texelFetch(g_source_tex, icoord, 0).rgb;

    if (g_out_color.b < 0.1) {
        vec3 color;
        float normalization = 0.0;

        color = texelFetch(g_source_tex, icoord - ivec2(1, 0), 0).rgb;
        g_out_color.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(g_source_tex, icoord + ivec2(1, 0), 0).rgb;
        g_out_color.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(g_source_tex, icoord - ivec2(0, 1), 0).rgb;
        g_out_color.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(g_source_tex, icoord + ivec2(0, 1), 0).rgb;
        g_out_color.rg += color.rg * color.b;
        normalization += color.b;

        g_out_color.rg /= normalization;
    }
}
