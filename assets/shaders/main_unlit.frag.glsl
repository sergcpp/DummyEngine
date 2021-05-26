#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
#extension GL_EXT_texture_cube_map_array : enable
#extension GL_ARB_bindless_texture: enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#ifdef GL_ES
    precision mediump float;
    precision mediump sampler2DShadow;
#endif

#include "internal/_fs_common.glsl"

#define LIGHT_ATTEN_CUTOFF 0.004

#if !defined(GL_ARB_bindless_texture)
layout(binding = REN_MAT_TEX0_SLOT) uniform sampler2D mat0_texture;
#endif // GL_ARB_bindless_texture

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 4) in vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
layout(location = 8) in flat uvec2 mat0_texture;
#endif // GL_ARB_bindless_texture
#else
in vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
in flat uvec2 mat0_texture;
#endif // GL_ARB_bindless_texture
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;

void main(void) {
    vec3 albedo_color = texture(SAMPLER2D(mat0_texture), aVertexUVs1_).rgb;
    
    outColor = vec4(albedo_color, 1.0);
    outNormal = vec4(0.0);
}
