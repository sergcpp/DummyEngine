precision mediump float;

/*
UNIFORMS
	col : 1
*/

uniform vec3 col;
varying vec3 aVertexNormal_;

void main(void) {
	gl_FragColor = vec4(col, 1.0) * 0.001 + vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);
}
