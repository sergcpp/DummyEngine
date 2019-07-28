R"(
#version 310 es

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
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
};

#if defined(MSAA_4)
layout(binding = 0) uniform highp sampler2DMS depth_texture;
#else
layout(binding = 0) uniform highp sampler2D depth_texture;
#endif
layout(binding = 1) uniform mediump sampler2D depth_low_texture;
layout(binding = 2) uniform lowp sampler2D source_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

float LinearizeDepth(float depth) {
    return uClipInfo[0] / (depth * (uClipInfo[1] - uClipInfo[2]) + uClipInfo[2]);
}

void main() {
    ivec2 icoord = ivec2(gl_FragCoord.xy);
    ivec2 icoord_low = ivec2(gl_FragCoord.xy) / 2;

    float d0 = LinearizeDepth(texelFetch(depth_texture, icoord, 0).r);
 
    float d1 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 0), 0).r);
    float d2 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(0, 1), 0).r);
    float d3 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 0), 0).r);
    float d4 = abs(d0 - texelFetch(depth_low_texture, icoord_low + ivec2(1, 1), 0).r);
 
    float dmin = min(min(d1, d2), min(d3, d4));
 
    if (dmin < 0.05) {
        outColor = texture(source_texture, aVertexUVs_ * uResAndFRes.xy / uResAndFRes.zw);
    } else {
        if (dmin == d1) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(0, 0), 0);
        }else if (dmin == d2) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(0, 1), 0);
        } else if (dmin == d3) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(1, 0), 0);
        } else if (dmin == d4) {
            outColor = texelFetch(source_texture, icoord_low + ivec2(1, 1), 0);
        }
    }
}
)"