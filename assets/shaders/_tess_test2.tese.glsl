#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable
#extension GL_GOOGLE_include_directive : enable

$ModifyWarning

#include "internal/_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(triangles, fractional_odd_spacing, ccw) in;
//layout(triangles, equal_spacing, ccw) in;

struct OutputPatch {
    vec3 aVertexPos_B030;
    vec3 aVertexPos_B021;
    vec3 aVertexPos_B012;
    vec3 aVertexPos_B003;
    vec3 aVertexPos_B102;
    vec3 aVertexPos_B201;
    vec3 aVertexPos_B300;
    vec3 aVertexPos_B210;
    vec3 aVertexPos_B120;
    vec3 aVertexPos_B111;
	vec2 aVertexUVs[3];
    vec3 aVertexNormal[3];
	vec3 aVertexTangent[3];
    //vec3 aVertexShUVs[3][4];
};

layout(location = 0) in patch OutputPatch oPatch;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_;
layout(location = 1) out mediump vec2 aVertexUVs_;
layout(location = 2) out mediump vec3 aVertexNormal_;
layout(location = 3) out mediump vec3 aVertexTangent_;
layout(location = 4) out highp vec3 aVertexShUVs_[4];
#else
out highp vec3 aVertexPos_;
out mediump vec2 aVertexUVs_;
out mediump vec3 aVertexNormal_;
out mediump vec3 aVertexTangent_;
out highp vec3 aVertexShUVs_[4];
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
	aVertexUVs_ = gl_TessCoord[0] * oPatch.aVertexUVs[0] + gl_TessCoord[1] * oPatch.aVertexUVs[1] + gl_TessCoord[2] * oPatch.aVertexUVs[2];
	aVertexNormal_ = gl_TessCoord[0] * oPatch.aVertexNormal[0] + gl_TessCoord[1] * oPatch.aVertexNormal[1] + gl_TessCoord[2] * oPatch.aVertexNormal[2];
	aVertexTangent_ = gl_TessCoord[0] * oPatch.aVertexTangent[0] + gl_TessCoord[1] * oPatch.aVertexTangent[1] + gl_TessCoord[2] * oPatch.aVertexTangent[2];
	
	//aVertexShUVs_[0] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][0] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][0] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][0];
	//aVertexShUVs_[1] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][1] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][1] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][1];
	//aVertexShUVs_[2] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][2] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][2] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][2];
	//aVertexShUVs_[3] = gl_TessCoord[0] * oPatch.aVertexShUVs[0][3] + gl_TessCoord[1] * oPatch.aVertexShUVs[1][3] + gl_TessCoord[2] * oPatch.aVertexShUVs[2][3];

	float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    float w = gl_TessCoord.z;

	float u_pow2 = u * u;
    float v_pow2 = v * v;
    float w_pow2 = w * w;
    float u_pow3 = u_pow2 * u;
    float v_pow3 = v_pow2 * v;
    float w_pow3 = w_pow2 * w;
    
	aVertexPos_ = oPatch.aVertexPos_B300 * w_pow3 +
                  oPatch.aVertexPos_B030 * u_pow3 +
                  oPatch.aVertexPos_B003 * v_pow3 +
                  oPatch.aVertexPos_B210 * 3.0 * w_pow2 * u +
                  oPatch.aVertexPos_B120 * 3.0 * w * u_pow2 +
                  oPatch.aVertexPos_B201 * 3.0 * w_pow2 * v +
                  oPatch.aVertexPos_B021 * 3.0 * u_pow2 * v +
                  oPatch.aVertexPos_B102 * 3.0 * w * v_pow2 +
                  oPatch.aVertexPos_B012 * 3.0 * u * v_pow2 +
                  oPatch.aVertexPos_B111 * 6.0 * w * u * v;

	gl_Position = shrd_data.uViewProjMatrix * vec4(aVertexPos_, 1.0);
} 
