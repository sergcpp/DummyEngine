R"(#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

)" __ADDITIONAL_DEFINES_STR__ R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
};

#if defined(MSAA_4)
layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform highp sampler2DMS depth_texture;
#else
layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform highp sampler2D depth_texture;
#endif

in vec2 aVertexUVs_;

out float outColor;

float LinearizeDepth(float depth) {
    return uClipInfo[0] / (depth * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
}

void main() {
    highp ivec2 coord = ivec2(aVertexUVs_);

    highp float d1 = texelFetch(depth_texture, coord + ivec2(0, 0), 0).r;
    highp float d2 = texelFetch(depth_texture, coord + ivec2(0, 1), 0).r;
    highp float d3 = texelFetch(depth_texture, coord + ivec2(1, 1), 0).r;
    highp float d4 = texelFetch(depth_texture, coord + ivec2(1, 0), 0).r;

    highp float max_depth = max(max(d1, d2), max(d3, d4));
    outColor = LinearizeDepth(max_depth);
}
)"