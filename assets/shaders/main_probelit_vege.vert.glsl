#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "common_vs.glsl

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    BatchDataBlock : $ubBatchDataLoc
*/

layout(location = $VtxPosLoc) in vec3 aVertexPosition;
layout(location = $VtxNorLoc) in vec4 aVertexNormal;
layout(location = $VtxTanLoc) in vec2 aVertexTangent;
layout(location = $VtxUV1Loc) in vec2 aVertexUVs1;
layout(location = $VtxAUXLoc) in uint aVertexColorPacked;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[$MaxProbes];
};

layout (location = $uInstancesLoc) uniform ivec4 uInstanceIndices[$MaxBatchSize / 4];

layout(binding = $InstanceBufSlot) uniform highp samplerBuffer instances_buffer;
layout(binding = $NoiseTexSlot) uniform sampler2D noise_texture;

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

#ifdef VULKAN
    #define gl_InstanceID gl_InstanceIndex
#endif

invariant gl_Position;

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    // load model matrix
    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

	// load vegetation properties
    vec4 veg_params = texelFetch(instances_buffer, instance * 4 + 3);

	vec3 vtx_pos_ls = aVertexPosition;
	vec4 vtx_color = unpackUnorm4x8(aVertexColorPacked);
	
	vec3 obj_pos_ws = MMatrix[3].xyz;
    vec4 wind_scroll = uWindScroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
	vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
	vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));
	
	vtx_pos_ls = TransformVegetation(vtx_pos_ls, vtx_color, wind_scroll, wind_params, wind_vec_ls, noise_texture);
	
	vec3 vtx_pos_ws = (MMatrix * vec4(vtx_pos_ls, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((MMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((MMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vtx_pos_ws;
    aVertexNormal_ = vtx_nor_ws;
    aVertexTangent_ = vtx_tan_ws;
    aVertexUVs_ = aVertexUVs1;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (uShadowMapRegions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }
    
    gl_Position = uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
