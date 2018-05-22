
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexNormal : 1
UNIFORMS
	uMVPMatrix : 0
	uMVMatrix : 1
*/

attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;

uniform mat4 uMVPMatrix;
uniform mat4 uMVMatrix;

varying vec3 aVertexPosition_;
varying vec3 aVertexNormal_;

void main(void) {
	aVertexPosition_ = uMVMatrix * vec4(aVertexPosition, 1.0);
    aVertexNormal_ = uMVMatrix * vec4(aVertexNormal, 0.0);
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
