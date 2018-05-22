#version 130

#ifdef GL_ES
precision mediump float;
#endif

/*
UNIFORMS
	col : 2
*/

uniform vec3 col;

in vec3 aVertexPosition_;
in vec3 aVertexNormal_;

void main(void) {
	gl_FragData[0] = vec4(aVertexPosition_, 1.0);
	gl_FragData[1] = vec4(aVertexNormal_, 0.0);
}
