#ifdef GL_ES
	precision mediump float;
#endif
	
/*
UNIFORMS
	mode : 2
	diffuse_texture : 3
	normals_texture : 4
	shadow_texture : 5
*/

uniform float mode;
uniform sampler2D diffuse_texture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;

varying mat3 aVertexTBN_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

varying vec4 aVertexShUVs_;

void main(void) {
	if (mode < 0.5) {
		vec3 frag_pos_ls = aVertexShUVs_.xyz / aVertexShUVs_.w;
		frag_pos_ls = frag_pos_ls * 0.5 + 0.5;
		//frag_pos_ls.x = 1.0 - frag_pos_ls.x;
        //frag_pos_ls = frag_pos_ls * 0.5;
		gl_FragColor = texture2D(diffuse_texture, aVertexUVs1_);
		gl_FragColor.xyz *= 0.001;

		vec3 normal = texture2D(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
		normal = aVertexTBN_ * normal;

		float lambert = dot(normal, vec3(0.0, 1.0, 0.0));
		if (lambert > 0.01) {
			float bias = max(0.05 * (1.0 - lambert), 0.005);  
			float frag_z_ls = texture2D(shadow_texture, frag_pos_ls.xy).r;
			if (frag_pos_ls.z - bias < frag_z_ls) {
				lambert = 0.0;
			}
		}

		gl_FragColor.xyz += lambert + 0.1;
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
