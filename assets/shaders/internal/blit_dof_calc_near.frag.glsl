#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"

layout(binding = REN_BASE0_TEX_SLOT) uniform sampler2D s_shrunk_coc;
layout(binding = REN_BASE1_TEX_SLOT) uniform sampler2D s_blurred_coc;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out float outCoc;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    float shrunk_coc = texelFetch(s_shrunk_coc, icoord, 0).r;
    float blurred_coc = texelFetch(s_blurred_coc, icoord, 0).r;

    float coc = 2.0 * max(blurred_coc, shrunk_coc) - shrunk_coc;
    outCoc = coc;
}
