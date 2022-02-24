#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable

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
layout(location = REN_VTX_AUX_LOC) in uint aVertexUnused;

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
layout(binding = REN_NOISE_TEX_SLOT) uniform sampler2D noise_texture;

layout(binding = REN_MATERIALS_SLOT) readonly buffer Materials {
    MaterialData materials[];
};

LAYOUT(location = 0) out highp vec3 aVertexPos_;
LAYOUT(location = 1) out mediump vec2 aVertexUVs_;
LAYOUT(location = 2) out mediump vec3 aVertexNormal_;
LAYOUT(location = 3) out mediump vec3 aVertexTangent_;
#if defined(BINDLESS_TEXTURES)
    LAYOUT(location = 9) out flat TEX_HANDLE norm_texture;
    LAYOUT(location = 10) out flat TEX_HANDLE spec_texture;
#endif // BINDLESS_TEXTURES

invariant gl_Position;

void main(void) {
    ivec2 instance = uInstanceIndices[gl_InstanceIndex];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance.x);

    vec3 vtx_pos_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vtx_pos_ws;
    aVertexNormal_ = vtx_nor_ws;
    aVertexTangent_ = vtx_tan_ws;
    aVertexUVs_ = aVertexUVs1;

#if defined(BINDLESS_TEXTURES)
    MaterialData mat = materials[instance.y];
    norm_texture = GET_HANDLE(mat.texture_indices[1]);
    spec_texture = GET_HANDLE(mat.texture_indices[2]);
#endif // BINDLESS_TEXTURES

    gl_Position = shrd_data.uViewProjMatrix * vec4(vtx_pos_ws, 1.0);
#if defined(VULKAN)
    gl_Position.y = -gl_Position.y;
#endif
}
