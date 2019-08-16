#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_gauss_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

layout(binding = SRC_TEX_SLOT) uniform sampler2D s_texture;

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

LAYOUT(location = 0) in vec2 aVertexUVs_;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.0);

    if(params.vertical.x < 0.5) {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(4, 0), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(1, 0), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(2, 0), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(3, 0), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(4, 0), 0) * 0.0162162162;
    } else {
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 4), 0) * 0.0162162162;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) - ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_), 0) * 0.2270270270;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 1), 0) * 0.1945945946;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 2), 0) * 0.1216216216;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 3), 0) * 0.0540540541;
        outColor += texelFetch(s_texture, ivec2(aVertexUVs_) + ivec2(0, 4), 0) * 0.0162162162;
    }
}

