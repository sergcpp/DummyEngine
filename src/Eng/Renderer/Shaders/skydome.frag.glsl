R"(#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

)" __ADDITIONAL_DEFINES_STR__ R"(

layout(binding = )" AS_STR(REN_BASE0_TEX_SLOT) R"() uniform samplerCube env_texture;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll, uWindScrollPrev;
};

in vec3 aVertexPos_;

layout(location = )" AS_STR(REN_OUT_COLOR_INDEX) R"() out vec4 outColor;
layout(location = )" AS_STR(REN_OUT_SPEC_INDEX) R"() out vec4 outSpecular;

vec3 RGBMDecode(vec4 rgbm) {
    return 4.0 * rgbm.rgb * rgbm.a;
}

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - uCamPosAndGamma.xyz);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 0.0);
}
)"
