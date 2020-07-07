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

layout (vertices = 3) out;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in highp vec3 aVertexPos_CS[];
layout(location = 1) in mediump vec2 aVertexUVs_CS[];
layout(location = 2) in mediump vec3 aVertexNormal_CS[];
layout(location = 3) in mediump vec3 aVertexTangent_CS[];
layout(location = 4) in highp vec3 aVertexShUVs_CS[][4];
#else
in highp vec3 aVertexPos_CS[];
in mediump vec2 aVertexUVs_CS[];
in mediump vec3 aVertexNormal_CS[];
in mediump vec3 aVertexTangent_CS[];
in highp vec3 aVertexShUVs_CS[][4];
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_ES[];
layout(location = 1) out mediump vec2 aVertexUVs_ES[];
layout(location = 2) out mediump vec3 aVertexNormal_ES[];
layout(location = 3) out mediump vec3 aVertexTangent_ES[];
layout(location = 4) out highp vec3 aVertexShUVs_ES[][4];
#else
out highp vec3 aVertexPos_ES[];
out mediump vec2 aVertexUVs_ES[];
out mediump vec3 aVertexNormal_ES[];
out mediump vec3 aVertexTangent_ES[];
out highp vec3 aVertexShUVs_ES[][4];
#endif

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

float GetTessLevel(float Distance0, float Distance1) {
    float AvgDistance = (Distance0 + Distance1) / 1.0;
	return min((32.0 * 1024.0) / (AvgDistance * AvgDistance), 64.0);
}

void main(void) {
    aVertexPos_ES[gl_InvocationID] = aVertexPos_CS[gl_InvocationID];
	aVertexUVs_ES[gl_InvocationID] = aVertexUVs_CS[gl_InvocationID];
	aVertexNormal_ES[gl_InvocationID] = aVertexNormal_CS[gl_InvocationID];
	aVertexTangent_ES[gl_InvocationID] = aVertexTangent_CS[gl_InvocationID];
	aVertexShUVs_ES[gl_InvocationID][0] = aVertexShUVs_CS[gl_InvocationID][0];
	aVertexShUVs_ES[gl_InvocationID][1] = aVertexShUVs_CS[gl_InvocationID][1];
	aVertexShUVs_ES[gl_InvocationID][2] = aVertexShUVs_CS[gl_InvocationID][2];
	aVertexShUVs_ES[gl_InvocationID][3] = aVertexShUVs_CS[gl_InvocationID][3];
	
#if 1
	float EyeToVertexDistance0 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_ES[0]);
    float EyeToVertexDistance1 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_ES[1]);
    float EyeToVertexDistance2 = distance(shrd_data.uCamPosAndGamma.xyz, aVertexPos_ES[2]);
	
	gl_TessLevelOuter[0] = GetTessLevel(EyeToVertexDistance1, EyeToVertexDistance2);
	gl_TessLevelOuter[1] = GetTessLevel(EyeToVertexDistance2, EyeToVertexDistance0);
	gl_TessLevelOuter[2] = GetTessLevel(EyeToVertexDistance0, EyeToVertexDistance1);
	gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
#else
	const float MaxTesselation = 64.0;
	const float ZoomFactor = 4.0;

	vec3 v0 = shrd_data.uCamPosAndGamma.xyz - aVertexPos_ES[0];
	vec3 v1 = shrd_data.uCamPosAndGamma.xyz - aVertexPos_ES[1];
	vec3 v2	= shrd_data.uCamPosAndGamma.xyz - aVertexPos_ES[2];
	
	float d0 = length(v0);
	float d1 = length(v1);
	float d2 = length(v2);

	float t0 = (1.0 - abs(dot(aVertexNormal_ES[0], v0) / d0)) * GetTessLevel(d1, d2);
	float t1 = (1.0 - abs(dot(aVertexNormal_ES[1], v1) / d1)) * GetTessLevel(d2, d0);
	float t2 = (1.0 - abs(dot(aVertexNormal_ES[2], v2) / d2)) * GetTessLevel(d0, d1);
	
	gl_TessLevelOuter[0] = t0;
	gl_TessLevelOuter[1] = t1;
	gl_TessLevelOuter[2] = t2;
	gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) * (1.0 / 3.0);
#endif
} 
