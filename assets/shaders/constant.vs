
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexNormal : 1
UNIFORMS
	uMVPMatrix : 0
*/

attribute vec3 aVertexPosition;
attribute vec3 aVertexNormal;
uniform mat4 uMVPMatrix;

varying vec3 aVertexNormal_;

void main(void) {
    aVertexNormal_ = aVertexNormal;
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
} 
