
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexNormal : 1
	aVertexTangent : 2
	aVertexUVs1 : 3
	aVertexUVs2 : 4
UNIFORMS
	uMVPMatrix : 0
	uShadowMatrix[0] : 1
*/

attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
attribute vec3 aVertexTangent;
attribute vec2 aVertexUVs1;
attribute vec2 aVertexUVs2;

uniform mat4 uMVPMatrix;
uniform mat4 uShadowMatrix[4];

varying mat3 aVertexTBN_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

varying vec4 aVertexShUVs_[4];

void main(void) {
	aVertexTBN_ = mat3(aVertexTangent, cross(aVertexNormal, aVertexTangent), aVertexNormal);
	aVertexUVs1_ = aVertexUVs1;
	aVertexUVs2_ = aVertexUVs2;
	
	aVertexShUVs_[0] = uShadowMatrix[0] * vec4(aVertexPosition, 1.0);
	aVertexShUVs_[1] = uShadowMatrix[1] * vec4(aVertexPosition, 1.0);
	aVertexShUVs_[2] = uShadowMatrix[2] * vec4(aVertexPosition, 1.0);
	aVertexShUVs_[3] = uShadowMatrix[3] * vec4(aVertexPosition, 1.0);

    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
