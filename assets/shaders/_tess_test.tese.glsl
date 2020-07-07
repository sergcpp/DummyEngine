#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "common_vs.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(triangles, fractional_odd_spacing, ccw) in;
//layout(triangles, equal_spacing, ccw) in;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_ES[];
layout(location = 1) in mediump vec2 aVertexUVs_ES[];
layout(location = 2) in mediump vec3 aVertexNormal_ES[];
layout(location = 3) in mediump vec3 aVertexTangent_ES[];
layout(location = 4) in highp vec3 aVertexShUVs_ES[][4];
#else
in highp vec3 aVertexPos_ES[];
in mediump vec2 aVertexUVs_ES[];
in mediump vec3 aVertexNormal_ES[];
in mediump vec3 aVertexTangent_ES[];
in highp vec3 aVertexShUVs_ES[][4];
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_;
layout(location = 1) out mediump vec2 aVertexUVs_;
layout(location = 2) out mediump vec3 aVertexNormal_;
layout(location = 3) out mediump vec3 aVertexTangent_;
layout(location = 4) out highp vec3 aVertexShUVs_[4];
layout(location = 8) out lowp float tex_height;
#else
out highp vec3 aVertexPos_;
out mediump vec2 aVertexUVs_;
out mediump vec3 aVertexNormal_;
out mediump vec3 aVertexTangent_;
out highp vec3 aVertexShUVs_[4];
out lowp float tex_height;
#endif

layout(binding = REN_MAT_TEX3_SLOT) uniform sampler2D bump_texture;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

void main(void) {
	aVertexPos_ = gl_TessCoord[0] * aVertexPos_ES[0] + gl_TessCoord[1] * aVertexPos_ES[1] + gl_TessCoord[2] * aVertexPos_ES[2];
	aVertexUVs_ = gl_TessCoord[0] * aVertexUVs_ES[0] + gl_TessCoord[1] * aVertexUVs_ES[1] + gl_TessCoord[2] * aVertexUVs_ES[2];
	aVertexNormal_ = gl_TessCoord[0] * aVertexNormal_ES[0] + gl_TessCoord[1] * aVertexNormal_ES[1] + gl_TessCoord[2] * aVertexNormal_ES[2];
	aVertexTangent_ = gl_TessCoord[0] * aVertexTangent_ES[0] + gl_TessCoord[1] * aVertexTangent_ES[1] + gl_TessCoord[2] * aVertexTangent_ES[2];
	
	aVertexShUVs_[0] = gl_TessCoord[0] * aVertexShUVs_ES[0][0] + gl_TessCoord[1] * aVertexShUVs_ES[1][0] + gl_TessCoord[2] * aVertexShUVs_ES[2][0];
	aVertexShUVs_[1] = gl_TessCoord[0] * aVertexShUVs_ES[0][1] + gl_TessCoord[1] * aVertexShUVs_ES[1][1] + gl_TessCoord[2] * aVertexShUVs_ES[2][1];
	aVertexShUVs_[2] = gl_TessCoord[0] * aVertexShUVs_ES[0][2] + gl_TessCoord[1] * aVertexShUVs_ES[1][2] + gl_TessCoord[2] * aVertexShUVs_ES[2][2];
	aVertexShUVs_[3] = gl_TessCoord[0] * aVertexShUVs_ES[0][3] + gl_TessCoord[1] * aVertexShUVs_ES[1][3] + gl_TessCoord[2] * aVertexShUVs_ES[2][3];

	float k = clamp((32.0 - distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_)) / 32.0, 0.0, 1.0);
	k *= k;
	k = 1.0;
	//float k = gl_TessLevelInner[0] / 64.0;

	//aVertexPos_.y += 4.0 * sin(aVertexPos_.x * 0.1);
	tex_height = 0.5 * texture(bump_texture, aVertexUVs_).r * k;
	aVertexPos_ += 1.0 * 0.05 * normalize(aVertexNormal_) * tex_height * k;

	gl_Position = shrd_data.uViewProjMatrix * vec4(aVertexPos_, 1.0);
} 
