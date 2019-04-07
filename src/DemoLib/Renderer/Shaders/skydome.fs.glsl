R"(
#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

layout(binding = )" AS_STR(REN_DIFF_TEX_SLOT) R"() uniform samplerCube env_texture;

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    mat4 uSunShadowMatrix[4];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
};

in vec3 aVertexPos_;

layout(location = )" AS_STR(REN_OUT_COLOR_INDEX) R"() out vec4 outColor;
layout(location = )" AS_STR(REN_OUT_SPEC_INDEX) R"() out vec4 outSpecular;

vec3 RGBMDecode(vec4 rgbm) {
    return 6.0 * rgbm.rgb * rgbm.a;
}

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - uCamPosAndGamma.xyz);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 1.0);
}
)"