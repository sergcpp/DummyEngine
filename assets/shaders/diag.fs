#ifdef GL_ES
	precision mediump float;
#endif
	
/*
UNIFORMS
	col : 1
	mode : 2
	diffuse_texture : 3
	normals_texture : 4
*/

uniform vec3 col;
uniform float mode;
uniform sampler2D diffuse_texture;
uniform sampler2D normals_texture;

varying mat3 aVertexTBN_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

void main(void) {
	if (mode < 0.5) {
		gl_FragColor = texture2D(diffuse_texture, aVertexUVs1_);
	} else if (mode < 1.5) {
		vec3 normal = aVertexTBN_[2]*0.5 + vec3(0.5);
		gl_FragColor = vec4(normal, 1.0);
	} else if (mode < 2.5) {
		vec3 tex_normal = texture2D(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
		gl_FragColor = vec4((aVertexTBN_ * tex_normal) * 0.5 + vec3(0.5), 1.0);
	} else if (mode < 3.5) {
		gl_FragColor = texture2D(diffuse_texture, aVertexUVs1_);
	} else if (mode < 4.5) {
		gl_FragColor = texture2D(diffuse_texture, aVertexUVs2_);
	}
}
