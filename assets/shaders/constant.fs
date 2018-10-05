#ifdef GL_ES
	precision mediump float;
#endif
	
/*
UNIFORMS
	diffuse_texture : 3
	normals_texture : 4
	shadow_texture : 5
    lightmap_texture : 6
	sun_dir : 10
	sun_col : 11
*/

uniform sampler2D diffuse_texture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;
uniform sampler2D lightmap_texture;

/*struct {

}*/

uniform vec3 sun_dir, sun_col;

varying mat3 aVertexTBN_;
varying vec2 aVertexUVs1_;
varying vec2 aVertexUVs2_;

varying vec4 aVertexShUVs_[4];

void main(void) {
	vec2 poisson_disk[64];
	poisson_disk[0] = vec2(-0.705374, -0.668203);
	poisson_disk[1] = vec2(-0.780145, 0.486251);
	poisson_disk[2] = vec2(0.566637, 0.605213);
	poisson_disk[3] = vec2(0.488876, -0.783441);
	poisson_disk[4] = vec2(-0.613392, 0.617481);
	poisson_disk[5] = vec2(0.170019, -0.040254);
	poisson_disk[6] = vec2(-0.299417, 0.791925);
	poisson_disk[7] = vec2(0.645680, 0.493210);
	poisson_disk[8] = vec2(-0.651784, 0.717887);
	poisson_disk[9] = vec2(0.421003, 0.027070);
	poisson_disk[10] = vec2(-0.817194, -0.271096);
	poisson_disk[11] = vec2(0.977050, -0.108615);
	poisson_disk[12] = vec2(0.063326, 0.142369);
	poisson_disk[13] = vec2(0.203528, 0.214331);
	poisson_disk[14] = vec2(-0.667531, 0.326090);
	poisson_disk[15] = vec2(-0.098422, -0.295755);
	poisson_disk[16] = vec2(-0.885922, 0.215369);
	poisson_disk[17] = vec2(0.039766, -0.396100);
	poisson_disk[18] = vec2(0.751946, 0.453352);
	poisson_disk[19] = vec2(0.078707, -0.715323);
	poisson_disk[20] = vec2(-0.075838, -0.529344);
	poisson_disk[21] = vec2(0.724479, -0.580798);
	poisson_disk[22] = vec2(0.222999, -0.215125);
	poisson_disk[23] = vec2(-0.467574, -0.405438);
	poisson_disk[24] = vec2(-0.248268, -0.814753);
	poisson_disk[25] = vec2(0.354411, -0.887570);
	poisson_disk[26] = vec2(0.175817, 0.382366);
	poisson_disk[27] = vec2(0.487472, -0.063082);
	poisson_disk[28] = vec2(-0.084078, 0.898312);
	poisson_disk[29] = vec2(0.470016, 0.217933);
	poisson_disk[30] = vec2(-0.696890, -0.549791);
	poisson_disk[31] = vec2(-0.149693, 0.605762);
	poisson_disk[32] = vec2(0.034211, 0.979980);
	poisson_disk[33] = vec2(0.503098, -0.308878);
	poisson_disk[34] = vec2(-0.016205, -0.872921);
	poisson_disk[35] = vec2(0.385784, -0.393902);
	poisson_disk[36] = vec2(-0.146886, -0.859249);
	poisson_disk[37] = vec2(0.643361, 0.164098);
	poisson_disk[38] = vec2(0.634388, -0.049471);
	poisson_disk[39] = vec2(-0.688894, 0.007843);
	poisson_disk[40] = vec2(0.464034, -0.188818);
	poisson_disk[41] = vec2(-0.440840, 0.137486);
	poisson_disk[42] = vec2(0.364483, 0.511704);
	poisson_disk[43] = vec2(0.034028, 0.325968);
	poisson_disk[44] = vec2(0.099094, -0.308023);
	poisson_disk[45] = vec2(0.693960, -0.366253);
	poisson_disk[46] = vec2(0.678884, -0.204688);
	poisson_disk[47] = vec2(0.001801, 0.780328);
	poisson_disk[48] = vec2(0.145177, -0.898984);
	poisson_disk[49] = vec2(0.062655, -0.611866);
	poisson_disk[50] = vec2(0.315226, -0.604297);
	poisson_disk[51] = vec2(-0.371868, 0.882138);
	poisson_disk[52] = vec2(0.200476, 0.494430);
	poisson_disk[53] = vec2(-0.494552, -0.711051);
	poisson_disk[54] = vec2(0.612476, 0.705252);
	poisson_disk[55] = vec2(-0.578845, -0.768792);
	poisson_disk[56] = vec2(-0.772454, -0.090976);
	poisson_disk[57] = vec2(0.504440, 0.372295);
	poisson_disk[58] = vec2(0.155736, 0.065157);
	poisson_disk[59] = vec2(0.391522, 0.849605);
	poisson_disk[60] = vec2(-0.620106, -0.328104);
	poisson_disk[61] = vec2(0.789239, -0.419965);
	poisson_disk[62] = vec2(-0.545396, 0.538133);
	poisson_disk[63] = vec2(-0.178564, -0.596057);

	vec3 frag_pos_ls[4];
	for (int i = 0; i < 4; i++) {
		frag_pos_ls[i] = 0.5 * aVertexShUVs_[i].xyz + 0.5;
		frag_pos_ls[i].xy *= 0.5;
	}

	vec3 normal = texture2D(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
	normal = aVertexTBN_ * normal;

	const float shadow_softness = 4.0 / 4096.0;

	vec3 color;

	float lambert = max(dot(normal, sun_dir), 0.0);
	float visibility = 1.0;
	if (lambert > 0.00001) {
		float bias = 0.0005 * tan(acos(lambert));//max(0.00125 * (1.0 - lambert), 0.00025);
		bias = clamp(bias, 0.00025, 0.001);

		float frag_depth = gl_FragCoord.z / gl_FragCoord.w;
		if (frag_depth < 8.0) {
			for (int i = 0; i < 64; i++) {
				float frag_z_ls = texture2D(shadow_texture, frag_pos_ls[0].xy + poisson_disk[i] * shadow_softness).r;
				if (frag_pos_ls[0].z - bias > frag_z_ls) {
					visibility -= 1.0/64.0;
				}
			}
			color = vec3(1.0, 0.0, 0.0);
		} else if (frag_depth < 24.0) {
			frag_pos_ls[1].x += 0.5;
			for (int i = 0; i < 16; i++) {
				float frag_z_ls = texture2D(shadow_texture, frag_pos_ls[1].xy + poisson_disk[i] * shadow_softness * 0.25).r;
				if (frag_pos_ls[1].z - bias*2 > frag_z_ls) {
					visibility -= 1.0/16.0;
				}
			}
			color = vec3(0.0, 1.0, 0.0);
		} else if (frag_depth < 56.0) {
			frag_pos_ls[2].y += 0.5;
			for (int i = 0; i < 4; i++) {
				float frag_z_ls = texture2D(shadow_texture, frag_pos_ls[2].xy + poisson_disk[i] * shadow_softness * 0.125).r;
				if (frag_pos_ls[2].z - bias * 4 > frag_z_ls) {
					visibility -= 1.0/4.0;
				}
			}
		} else if (frag_depth < 120.0) {
			frag_pos_ls[3].xy += 0.5;
			float frag_z_ls = texture2D(shadow_texture, frag_pos_ls[3].xy).r;
			if (frag_pos_ls[3].z - bias * 8 > frag_z_ls) {
				visibility -= 1.0;
			}
		} else {
			// use directional lightmap here
		}
	}
    
    const float gamma = 2.2;
    
    vec3 indirect_col = texture2D(lightmap_texture, vec2(aVertexUVs2_.x, 1.0 - aVertexUVs2_.y)).rgb;

	vec3 diffuse_color = pow(texture2D(diffuse_texture, aVertexUVs1_).rgb, vec3(gamma)) * (sun_col * lambert * visibility + indirect_col);
    diffuse_color = pow(diffuse_color, vec3(1.0/gamma));
    
	gl_FragColor = vec4(diffuse_color, 1.0);
}
