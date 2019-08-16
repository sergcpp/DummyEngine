#version 310 es

#if defined(GL_ES) || defined(VULKAN)
	precision highp int;
	precision mediump float;
#endif

#include "_fs_common.glsl"
#include "skydome_interface.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = ENV_TEX_SLOT) uniform samplerCube env_texture;

LAYOUT(location = 0) in highp vec3 aVertexPos_;

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main() {
    vec3 view_dir_ws = normalize(aVertexPos_ - shrd_data.uCamPosAndGamma.xyz);

    outColor.rgb = clamp(RGBMDecode(texture(env_texture, view_dir_ws)), vec3(0.0), vec3(16.0));
    outColor.a = 1.0;
    outSpecular = vec4(0.0, 0.0, 0.0, 0.0);
}

