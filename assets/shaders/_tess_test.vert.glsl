#version 310 es
#extension GL_EXT_texture_buffer : enable
#extension GL_OES_texture_buffer : enable
//#extension GL_EXT_control_flow_attributes : enable

$ModifyWarning

#include "internal/_vs_common.glsl"

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
layout(location = REN_VTX_NOR_LOC) in vec4 aVertexNormal;
layout(location = REN_VTX_TAN_LOC) in vec2 aVertexTangent;
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
layout(location = REN_VTX_AUX_LOC) in vec2 aVertexUnused;

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = 0, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout (location = REN_U_INSTANCES_LOC) uniform ivec4 uInstanceIndices[REN_MAX_BATCH_SIZE / 4];
layout (location = REN_U_MAT_PARAM_LOC) uniform vec4 uMaterialParams;

layout(binding = REN_INST_BUF_SLOT) uniform highp samplerBuffer instances_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) out highp vec3 aVertexPos_CS;
layout(location = 1) out mediump vec2 aVertexUVs_CS;
layout(location = 2) out mediump vec3 aVertexNormal_CS;
layout(location = 3) out mediump vec3 aVertexTangent_CS;
layout(location = 4) out highp vec3 aVertexShUVs_CS[4];
#else
out highp vec3 aVertexPos_CS;
out mediump vec2 aVertexUVs_CS;
out mediump vec3 aVertexNormal_CS;
out mediump vec3 aVertexTangent_CS;
out highp vec3 aVertexShUVs_CS[4];
#endif

#ifdef VULKAN
    #define gl_InstanceID gl_InstanceIndex
#endif

//invariant gl_Position;

void main(void) {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance);

    vec3 vtx_pos_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vtx_nor_ws = normalize((model_matrix * vec4(aVertexNormal.xyz, 0.0)).xyz);
    vec3 vtx_tan_ws = normalize((model_matrix * vec4(aVertexNormal.w, aVertexTangent, 0.0)).xyz);

    aVertexPos_CS = vtx_pos_ws;
    aVertexNormal_CS = vtx_nor_ws;
    aVertexTangent_CS = vtx_tan_ws;
    aVertexUVs_CS = aVertexUVs1;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    /*[[unroll]]*/ for (int i = 0; i < 4; i++) {
        aVertexShUVs_CS[i] = (shrd_data.uShadowMapRegions[i].clip_from_world *
                            vec4(vtx_pos_ws, 1.0)).xyz;
        aVertexShUVs_CS[i] = 0.5 * aVertexShUVs_CS[i] + 0.5;
        aVertexShUVs_CS[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_CS[i].xy += offsets[i];
    }
} 
