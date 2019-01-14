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
layout(location = 1) in vec3 aVertexNormal;
layout(location = 2) in vec3 aVertexTangent;
layout(location = 3) in vec2 aVertexUVs1;
layout(location = 4) in vec2 aVertexUVs2;

layout (std140) uniform MatricesBlock {
    mat4 uMVPMatrix;
    mat4 uMVMatrix;
    mat4 uShadowMatrix[4];
};

out vec3 aVertexPos_;
out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec2 aVertexUVs2_;

out vec4 aVertexShUVs_[4];

void main(void) {
    vec3 vertex_position_ws = (uMVMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vertex_normal_ws = (uMVMatrix * vec4(aVertexNormal, 0.0)).xyz;
    vec3 vertex_tangent_ws = (uMVMatrix * vec4(aVertexTangent, 0.0)).xyz;

    aVertexPos_ = vertex_position_ws;
    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    aVertexUVs2_ = aVertexUVs2;
    
    aVertexShUVs_[0] = uShadowMatrix[0] * vec4(aVertexPosition, 1.0);
    aVertexShUVs_[1] = uShadowMatrix[1] * vec4(aVertexPosition, 1.0);
    aVertexShUVs_[2] = uShadowMatrix[2] * vec4(aVertexPosition, 1.0);
    aVertexShUVs_[3] = uShadowMatrix[3] * vec4(aVertexPosition, 1.0);

    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
