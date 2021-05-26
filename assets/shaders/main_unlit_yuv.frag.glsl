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
layout(binding = REN_MAT_TEX1_SLOT) uniform sampler2D mat1_texture;
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
layout(location = 9) in flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#else
in vec2 aVertexUVs1_;
#if defined(GL_ARB_bindless_texture)
in flat uvec2 mat0_texture;
in flat uvec2 mat1_texture;
#endif // GL_ARB_bindless_texture
#endif

layout(location = REN_OUT_COLOR_INDEX) out vec4 outColor;
layout(location = REN_OUT_NORM_INDEX) out vec4 outNormal;

void main(void) {
	vec3 col_yuv;
	col_yuv.x = texture(SAMPLER2D(mat0_texture), aVertexUVs1_).r;
	col_yuv.yz = texture(SAMPLER2D(mat1_texture), aVertexUVs1_).rg;
	col_yuv += vec3(-0.0627451017, -0.501960814, -0.501960814);
    
	vec3 col_rgb;
	col_rgb.r = dot(col_yuv, vec3(1.164,  0.000,  1.596));
	col_rgb.g = dot(col_yuv, vec3(1.164, -0.391, -0.813));
	col_rgb.b = dot(col_yuv, vec3(1.164,  2.018,  0.000));
	
    outColor = vec4(SRGBToLinear(col_rgb), 1.0);
    outNormal = vec4(0.0);
}
