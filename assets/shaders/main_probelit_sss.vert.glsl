#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"
#include "internal/_texturing.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
layout(location = REN_VTX_NOR_LOC) in vec4 aVertexNormal;
layout(location = REN_VTX_TAN_LOC) in vec2 aVertexTangent;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
layout(location = REN_VTX_AUX_LOC) in uint aVertexColorPacked;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

#if defined(VULKAN)
layout(push_constant) uniform PushConstants {
    ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
};
#else
layout(location = REN_U_INSTANCES_LOC) uniform ivec2 uInstanceIndices[REN_MAX_BATCH_SIZE];
#endif

layout(binding = REN_INST_BUF_SLOT) uniform samplerBuffer instances_buffer;

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
    MaterialData materials[];
};

LAYOUT(location = 0) out highp vec3 aVertexPos_;
LAYOUT(location = 1) out mediump vec3 aVertexUVAndCurvature_;
LAYOUT(location = 2) out mediump vec3 aVertexNormal_;
LAYOUT(location = 3) out mediump vec3 aVertexTangent_;
LAYOUT(location = 4) out highp vec3 aVertexShUVs_[4];
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 8) out flat TEX_HANDLE diff_texture;
    LAYOUT(location = 9) out flat TEX_HANDLE norm_texture;
    LAYOUT(location = 10) out flat TEX_HANDLE spec_texture;
    LAYOUT(location = 11) out flat TEX_HANDLE sss_texture;
    LAYOUT(location = 12) out flat TEX_HANDLE norm_detail_texture;
#endif // BINDLESS_TEXTURES
LAYOUT(location = 13) out flat vec4 material_params;

invariant gl_Position;

void main(void) {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance.x);

    vec3 vtx_pos_ls = aVertexPosition;
    float vtx_curvature = unpackUnorm4x8(aVertexColorPacked).r;

    vec3 vtx_pos_ws = (model_matrix * vec4(vtx_pos_ls, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vtx_pos_ws;
    aVertexNormal_ = vtx_nor_ws;
    aVertexTangent_ = vtx_tan_ws;
    aVertexUVAndCurvature_ = vec3(aVertexUVs1, vtx_curvature);

    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );

    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (shrd_data.uShadowMapRegions[i].clip_from_world * vec4(vtx_pos_ws, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }

    MaterialData mat = materials[instance.y];
    material_params = mat.params;
#if defined(BINDLESS_TEXTURES)
    diff_texture = GET_HANDLE(mat.texture_indices[0]);
    norm_texture = GET_HANDLE(mat.texture_indices[1]);
    spec_texture = GET_HANDLE(mat.texture_indices[2]);
    sss_texture = GET_HANDLE(mat.texture_indices[3]);
    norm_detail_texture = GET_HANDLE(mat.texture_indices[4]);
#endif // BINDLESS_TEXTURES

    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
