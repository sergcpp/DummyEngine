#version 310 es
#extension GL_EXT_texture_buffer : enable

$ModifyWarning

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = $VtxPosLoc) in vec3 aVertexPosition;
layout(location = $VtxNorLoc) in vec3 aVertexNormal;
layout(location = $VtxTanLoc) in vec3 aVertexTangent;
layout(location = $VtxUV1Loc) in vec2 aVertexUVs1;
layout(location = $VtxUV2Loc) in vec2 aVertexUVs2;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[$MaxShadowMaps];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
};

layout(binding = $InstanceBufSlot) uniform mediump samplerBuffer instances_buffer;

layout(location = $uInstancesLoc) uniform int uInstanceIndices[8];

out vec3 aVertexPos_;
out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec2 aVertexUVs2_;

out vec3 aVertexShUVs_[4];

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID];

    // load model matrix
    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

    // load lightmap transform
    vec4 LightmapTr = texelFetch(instances_buffer, instance * 4 + 3);
    
    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vertex_normal_ws = normalize((MMatrix * vec4(aVertexNormal, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((MMatrix * vec4(aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vertex_position_ws;
    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    aVertexUVs2_ = LightmapTr.xy + LightmapTr.zw * aVertexUVs2;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (uShadowMapRegions[i].clip_from_world * MMatrix * vec4(aVertexPosition, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }
    
    gl_Position = uViewProjMatrix * MMatrix * vec4(aVertexPosition, 1.0);
} 
