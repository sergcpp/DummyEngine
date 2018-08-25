
/*
ATTRIBUTES
	aVertexPosition : 0
	aVertexUVs : 2
UNIFORMS
	uMVPMatrix : 0
*/

attribute vec3 aVertexPosition;

uniform mat4 uMVPMatrix;

varying vec4 aVertexPosition_;

void main(void) {
	gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
    aVertexPosition_ = gl_Position;
} 

