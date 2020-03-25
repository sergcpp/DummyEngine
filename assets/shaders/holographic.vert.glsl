#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
    BatchDataBlock : $ubBatchDataLoc
*/

layout(location = $VtxPosLoc) in vec3 aVertexPosition;
layout(location = $VtxNorLoc) in vec4 aVertexNormal;
layout(location = $VtxTanLoc) in vec2 aVertexTangent;
layout(location = $VtxUV1Loc) in vec2 aVertexUVs1;
layout(location = $VtxAUXLoc) in uint aVertexUnused;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

struct ProbeItem {
    vec4 pos_and_radius;
    vec4 unused_and_layer;
    vec4 sh_coeffs[3];
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
	vec4 uWindScroll, uWindScrollPrev;
    ProbeItem uProbes[$MaxProbes];
};

layout (location = $uInstancesLoc) uniform ivec4 uInstanceIndices[$MaxBatchSize / 4];

layout(binding = $InstanceBufSlot) uniform mediump samplerBuffer instances_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_;
layout(location = 1) out mediump vec2 aVertexUVs_;
layout(location = 2) out mediump vec3 aVertexNormal_;
layout(location = 3) out mediump vec3 aVertexTangent_;
#else
out highp vec3 aVertexPos_;
out mediump vec2 aVertexUVs_;
out mediump vec3 aVertexNormal_;
out mediump vec3 aVertexTangent_;
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

    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vertex_normal_ws = normalize((MMatrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((MMatrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vertex_position_ws;
    aVertexNormal_ = vertex_normal_ws;
    aVertexTangent_ = vertex_tangent_ws;
    aVertexUVs_ = aVertexUVs1;
    
    gl_Position = uViewProjMatrix * MMatrix * vec4(aVertexPosition, 1.0);
} 
