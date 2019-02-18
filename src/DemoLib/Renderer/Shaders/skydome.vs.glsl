R"(
#version 310 es

/*
UNIFORMS
    uMVPMatrix : 0
    uMVMatrix : 1
    uShadowMatrix[0] : 2
UNIFORM_BLOCKS
    MatricesBlock : 0
*/

layout(location = 0) in vec3 aVertexPosition;

layout (std140) uniform MatricesBlock {
    mat4 uMVPMatrix;
    mat4 uVMatrix;
    mat4 uMMatrix;
    mat4 uShadowMatrix[4];
};

out vec3 aVertexPos_;

void main() {
    vec3 vertex_position_ws = (uMMatrix * vec4(aVertexPosition, 1.0)).xyz;
    aVertexPos_ = vertex_position_ws;

    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
)"