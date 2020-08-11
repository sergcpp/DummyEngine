#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

#include "_fs_common.glsl"

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform samplerCube env_texture;

in vec3 aVertexPos_;

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 0.0);
}

