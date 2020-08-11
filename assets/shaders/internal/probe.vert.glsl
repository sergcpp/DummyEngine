#version 310 es

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#include "_vs_common.glsl"

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(location = REN_VTX_POS_LOC) in vec3 aVertexPosition;

layout(location = REN_U_M_MATRIX_LOC) uniform mat4 uMMatrix;

out vec3 aVertexPos_;

void main() {
    vec3 vertex_position_ws = (uMMatrix * vec4(aVertexPosition, 1.0)).xyz;
    aVertexPos_ = vertex_position_ws;

    gl_Position = shrd_data.uViewProjMatrix * uMMatrix * vec4(aVertexPosition, 1.0);
} 
