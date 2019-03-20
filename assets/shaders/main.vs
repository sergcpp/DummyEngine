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
    mat4 uVMatrix;
    mat4 uMMatrix;
    mat4 uShadowMatrix[4];
    vec4 uClipInfo;
};

out vec3 aVertexPos_;
out mat3 aVertexTBN_;
out vec2 aVertexUVs1_;
out vec2 aVertexUVs2_;

out vec3 aVertexShUVs_[4];

void main(void) {
    vec3 vertex_position_ws = (uMMatrix * vec4(aVertexPosition, 1.0)).xyz;
    vec3 vertex_normal_ws = normalize((uMMatrix * vec4(aVertexNormal, 0.0)).xyz);
    vec3 vertex_tangent_ws = normalize((uMMatrix * vec4(aVertexTangent, 0.0)).xyz);

    aVertexPos_ = vertex_position_ws;
    aVertexTBN_ = mat3(vertex_tangent_ws, cross(vertex_normal_ws, vertex_tangent_ws), vertex_normal_ws);
    aVertexUVs1_ = aVertexUVs1;
    aVertexUVs2_ = aVertexUVs2;
    
    const vec2 offsets[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(0.25, 0.0),
        vec2(0.0, 0.5),
        vec2(0.25, 0.5)
    );
    
    for (int i = 0; i < 4; i++) {
        aVertexShUVs_[i] = (uShadowMatrix[i] * vec4(aVertexPosition, 1.0)).xyz;
        aVertexShUVs_[i] = 0.5 * aVertexShUVs_[i] + 0.5;
        aVertexShUVs_[i].xy *= vec2(0.25, 0.5);
        aVertexShUVs_[i].xy += offsets[i];
    }
    
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
