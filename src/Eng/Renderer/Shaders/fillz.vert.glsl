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

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;
#ifdef TRANSPARENT_PERM
layout(location = REN_VTX_UV1_LOC) in vec2 aVertexUVs1;
#endif

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_INST_BUF_SLOT) uniform mediump samplerBuffer instances_buffer;
layout(location = REN_U_INSTANCES_LOC) uniform ivec4 uInstanceIndices[REN_MAX_BATCH_SIZE / 4];

#ifdef TRANSPARENT_PERM
out vec2 aVertexUVs1_;
out vec3 aVertexObjCoord_;
#endif

invariant gl_Position;

void main() {
    int instance = uInstanceIndices[gl_InstanceID / 4][gl_InstanceID % 4];

    mat4 model_matrix = FetchModelMatrix(instances_buffer, instance);

#ifdef TRANSPARENT_PERM
    aVertexUVs1_ = aVertexUVs1;
    aVertexObjCoord_ = aVertexPosition;
#endif

    vec3 vertex_position_ws = (model_matrix * vec4(aVertexPosition, 1.0)).xyz;
    gl_Position = shrd_data.uViewProjMatrix * vec4(vertex_position_ws, 1.0);
} 
)"