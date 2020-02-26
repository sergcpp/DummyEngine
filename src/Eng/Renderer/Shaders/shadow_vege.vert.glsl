R"(#version 310 es
#extension GL_EXT_texture_buffer : enable

)" __ADDITIONAL_DEFINES_STR__ R"(

)"
#include "_vs_common.glsl"
R"(

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

layout(location = )" AS_STR(REN_VTX_POS_LOC) R"() in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = )" AS_STR(REN_VTX_UV1_LOC) R"() in vec2 aVertexUVs1;
#endif
//layout(location = )" AS_STR(REN_VTX_NOR_LOC) R"() in vec3 aVertexNormal;
layout(location = )" AS_STR(REN_VTX_AUX_LOC) R"() in uint aVertexColorPacked;

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes, uTranspParamsAndTime;
    vec4 uWindScroll;
};

layout(binding = )" AS_STR(REN_INST_BUF_SLOT) R"() uniform highp samplerBuffer instances_buffer;
layout(binding = )" AS_STR(REN_NOISE_TEX_SLOT) R"() uniform sampler2D noise_texture;

layout(location = )" AS_STR(REN_U_M_MATRIX_LOC) R"() uniform mat4 uShadowViewProjMatrix;
layout(location = )" AS_STR(REN_U_INSTANCES_LOC) R"() uniform ivec4 uInstanceIndices[)" AS_STR(REN_MAX_BATCH_SIZE) R"( / 4];

#ifdef TRANSPARENT_PERM
out vec2 aVertexUVs1_;
#endif

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 MMatrix;
    MMatrix[0] = texelFetch(instances_buffer, instance * 4 + 0);
    MMatrix[1] = texelFetch(instances_buffer, instance * 4 + 1);
    MMatrix[2] = texelFetch(instances_buffer, instance * 4 + 2);
    MMatrix[3] = vec4(0.0, 0.0, 0.0, 1.0);

    MMatrix = transpose(MMatrix);

    vec4 veg_params = texelFetch(instances_buffer, instance * 4 + 3);

    vec3 vtx_pos_ls = aVertexPosition;
	vec4 vtx_color = unpackUnorm4x8(aVertexColorPacked);

    vec3 obj_pos_ws = MMatrix[3].xyz;
    vec4 wind_scroll = uWindScroll + vec4(VEGE_NOISE_SCALE_LF * obj_pos_ws.xz, VEGE_NOISE_SCALE_HF * obj_pos_ws.xz);
	vec4 wind_params = unpackUnorm4x8(floatBitsToUint(veg_params.x));
	vec4 wind_vec_ls = vec4(unpackHalf2x16(floatBitsToUint(veg_params.y)), unpackHalf2x16(floatBitsToUint(veg_params.z)));

    vtx_pos_ls = TransformVegetation(vtx_pos_ls, vtx_color, wind_scroll, wind_params, wind_vec_ls, noise_texture);

    vec3 vtx_pos_ws = (MMatrix * vec4(vtx_pos_ls, 1.0)).xyz;

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
#endif

    gl_Position = uShadowViewProjMatrix * vec4(vtx_pos_ws, 1.0);
} 
)"