R"(#version 310 es
#extension GL_EXT_texture_buffer : enable

)" __ADDITIONAL_DEFINES_STR__ R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = )" AS_STR(REN_VTX_UV1_LOC) R"() in vec2 aVertexUVs1;
#endif

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix, uViewProjPrevMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol, uTaaInfo;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll, uWindScrollPrev;
};

layout(binding = )" AS_STR(REN_INST_BUF_SLOT) R"() uniform mediump samplerBuffer instances_buffer;
layout(location = )" AS_STR(REN_U_INSTANCES_LOC) R"() uniform ivec4 uInstanceIndices[)" AS_STR(REN_MAX_BATCH_SIZE) R"( / 4];

#ifdef TRANSPARENT_PERM
out vec2 aVertexUVs1_;
#endif

invariant gl_Position;

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
#endif

    vec3 vertex_position_ws = (MMatrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = uViewProjMatrix * vec4(vertex_position_ws, 1.0);
} 
)"