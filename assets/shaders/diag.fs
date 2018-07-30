precision mediump float;

/*
UNIFORMS
	col : 1
	mode : 2
	checker_texture : 3
*/

uniform vec3 col;
uniform float mode;
uniform sampler2D checker_texture;

varying vec3 aVertexNormal_;
varying vec3 aVertexTangent_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

void main(void) {
	if (mode < 0.5) {
		gl_FragColor = vec4(col, 1.0) * 0.001 + vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0) * 0.001 + vec4(1.0, 0.0, 0.0, 1.0);
	} else if (mode < 1.5) {
		gl_FragColor = vec4(aVertexNormal_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	} else if (mode < 2.5) {
		gl_FragColor = vec4(aVertexTangent_*0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	} else if (mode < 3.5) {
		gl_FragColor = texture2D(checker_texture, aVertexUVs1_);
	} else if (mode < 4.5) {
		gl_FragColor = texture2D(checker_texture, aVertexUVs2_);
	}
}
