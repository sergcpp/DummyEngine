
/*
ATTRIBUTES
    aVertexPosition : 0
    aVertexNormal : 1
    aVertexTangent : 2
    aVertexUVs1 : 3
    aVertexUVs2 : 4
UNIFORMS
    uMVPMatrix : 0
*/

attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec3 aVertexTangent;
attribute vec2 aVertexUVs1;
attribute vec2 aVertexUVs2;

uniform mat4 uMVPMatrix;

varying mat3 aVertexTBN_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

void main(void) {
    aVertexTBN_ = mat3(aVertexTangent, cross(aVertexNormal, aVertexTangent), aVertexNormal);
    aVertexUVs1_ = aVertexUVs1;
    aVertexUVs2_ = aVertexUVs2;
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
