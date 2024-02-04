#version 310 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D g_shrunk_coc;
layout(binding = REN_BASE1_TEX_SLOT) uniform sampler2D g_blurred_coc;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out float outCoc;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

    float shrunk_coc = texelFetch(g_shrunk_coc, icoord, 0).r;
    float blurred_coc = texelFetch(g_blurred_coc, icoord, 0).r;

    float coc = 2.0 * max(blurred_coc, shrunk_coc) - shrunk_coc;
    outCoc = coc;
}
