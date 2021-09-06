#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssr_dilate_interface.glsl"
        
/*
UNIFORM_BLOCKS
	UniformParams : $ubUnifParamLoc
*/

layout(binding = SSR_TEX_SLOT) uniform highp sampler2D source_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec3 outColor;

void main() {
    ivec2 icoord = ivec2(aVertexUVs_);

    outColor = texelFetch(source_texture, icoord, 0).rgb;

    if (outColor.b < 0.1) {
        vec3 color;
        float normalization = 0.0;

        color = texelFetch(source_texture, icoord - ivec2(1, 0), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord + ivec2(1, 0), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord - ivec2(0, 1), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        color = texelFetch(source_texture, icoord + ivec2(0, 1), 0).rgb;
        outColor.rg += color.rg * color.b;
        normalization += color.b;

        outColor.rg /= normalization;
    }
}
