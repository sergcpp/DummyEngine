#version 430 core

#include "_fs_common.glsl"

layout(binding = BIND_BASE0_TEX) uniform sampler2D g_shrunk_coc;
layout(binding = BIND_BASE1_TEX) uniform sampler2D g_blurred_coc;

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out float outCoc;

void main() {
    ivec2 icoord = ivec2(g_vtx_uvs);

    float shrunk_coc = texelFetch(g_shrunk_coc, icoord, 0).x;
    float blurred_coc = texelFetch(g_blurred_coc, icoord, 0).x;

    float coc = 2.0 * max(blurred_coc, shrunk_coc) - shrunk_coc;
    outCoc = coc;
}
