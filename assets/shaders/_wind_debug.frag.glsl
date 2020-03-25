#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "common_fs.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D diffuse_texture;
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D normals_texture;
layout(binding = REN_MAT_TEX2_SLOT) uniform sampler2D specular_texture;
layout(binding = REN_SHAD_TEX_SLOT) uniform sampler2DShadow shadow_texture;
layout(binding = REN_DECAL_TEX_SLOT) uniform sampler2D decals_texture;
layout(binding = REN_SSAO_TEX_SLOT) uniform sampler2D ao_texture;
layout(binding = REN_LIGHT_BUF_SLOT) uniform mediump samplerBuffer lights_buffer;
layout(binding = REN_DECAL_BUF_SLOT) uniform mediump samplerBuffer decals_buffer;
layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;
layout(binding = REN_INST_BUF_SLOT) uniform sampler2D noise_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_;
layout(location = 1) in mediump vec2 aVertexUVs_;
layout(location = 2) in mediump vec3 aVertexNormal_;
layout(location = 3) in mediump vec3 aVertexTangent_;
layout(location = 4) in highp vec3 aVertexShUVs_[4];
#else
in highp vec3 aVertexPos_;
in mediump vec2 aVertexUVs_;
in mediump vec3 aVertexNormal_;
in mediump vec3 aVertexTangent_;
in highp vec3 aVertexShUVs_[4];
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;
layout(location = REN_OUT_SPEC_INDEX) out vec4 outSpecular;

void main(void) {
	vec3 lo_freq_bend_dir = 0.5 * texture(noise_texture, shrd_data.uWindScroll.xy + (1.0 / 256.0) * vec2(aVertexPos_.xz)).rgb + vec3(0.5);
	vec3 hi_freq_bend_dir = 0.5 * texture(noise_texture, shrd_data.uWindScroll.zw + (1.0 / 8.0) * vec2(aVertexPos_.xz)).rgb + vec3(0.5);

    outColor = vec4(pow(lo_freq_bend_dir + hi_freq_bend_dir, vec3(2.2)), 1.0);
    outNormal = vec4(0.0);
    outSpecular = vec4(0.0);
}
