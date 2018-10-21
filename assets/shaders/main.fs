#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif
    
/*
UNIFORMS
    diffuse_texture : 3
    normals_texture : 4
    shadow_texture : 5
    lm_direct_texture : 6
    lm_indirect_texture : 7
    sun_dir : 10
    sun_col : 11
    gamma : 12
    
*/

uniform sampler2D diffuse_texture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;
uniform sampler2D lm_direct_texture;
uniform sampler2D lm_indirect_texture;

/*struct {

}*/

uniform vec3 sun_dir, sun_col;
uniform float gamma;

in mat3 aVertexTBN_;
in vec2 aVertexUVs1_;
in vec2 aVertexUVs2_;

in vec4 aVertexShUVs_[4];

out vec4 outColor;

void main(void) {
    const vec2 poisson_disk[64] = vec2[64](
        vec2(-0.705374, -0.668203),
        vec2(-0.780145, 0.486251),
        vec2(0.566637, 0.605213),
        vec2(0.488876, -0.783441),
        vec2(-0.613392, 0.617481),
        vec2(0.170019, -0.040254),
        vec2(-0.299417, 0.791925),
        vec2(0.645680, 0.493210),
        vec2(-0.651784, 0.717887),
        vec2(0.421003, 0.027070),
        vec2(-0.817194, -0.271096),
        vec2(0.977050, -0.108615),
        vec2(0.063326, 0.142369),
        vec2(0.203528, 0.214331),
        vec2(-0.667531, 0.326090),
        vec2(-0.098422, -0.295755),
        vec2(-0.885922, 0.215369),
        vec2(0.039766, -0.396100),
        vec2(0.751946, 0.453352),
        vec2(0.078707, -0.715323),
        vec2(-0.075838, -0.529344),
        vec2(0.724479, -0.580798),
        vec2(0.222999, -0.215125),
        vec2(-0.467574, -0.405438),
        vec2(-0.248268, -0.814753),
        vec2(0.354411, -0.887570),
        vec2(0.175817, 0.382366),
        vec2(0.487472, -0.063082),
        vec2(-0.084078, 0.898312),
        vec2(0.470016, 0.217933),
        vec2(-0.696890, -0.549791),
        vec2(-0.149693, 0.605762),
        vec2(0.034211, 0.979980),
        vec2(0.503098, -0.308878),
        vec2(-0.016205, -0.872921),
        vec2(0.385784, -0.393902),
        vec2(-0.146886, -0.859249),
        vec2(0.643361, 0.164098),
        vec2(0.634388, -0.049471),
        vec2(-0.688894, 0.007843),
        vec2(0.464034, -0.188818),
        vec2(-0.440840, 0.137486),
        vec2(0.364483, 0.511704),
        vec2(0.034028, 0.325968),
        vec2(0.099094, -0.308023),
        vec2(0.693960, -0.366253),
        vec2(0.678884, -0.204688),
        vec2(0.001801, 0.780328),
        vec2(0.145177, -0.898984),
        vec2(0.062655, -0.611866),
        vec2(0.315226, -0.604297),
        vec2(-0.371868, 0.882138),
        vec2(0.200476, 0.494430),
        vec2(-0.494552, -0.711051),
        vec2(0.612476, 0.705252),
        vec2(-0.578845, -0.768792),
        vec2(-0.772454, -0.090976),
        vec2(0.504440, 0.372295),
        vec2(0.155736, 0.065157),
        vec2(0.391522, 0.849605),
        vec2(-0.620106, -0.328104),
        vec2(0.789239, -0.419965),
        vec2(-0.545396, 0.538133),
        vec2(-0.178564, -0.596057)
    );

    vec3 frag_pos_ls[4];
    for (int i = 0; i < 4; i++) {
        frag_pos_ls[i] = 0.5 * aVertexShUVs_[i].xyz + 0.5;
        frag_pos_ls[i].xy *= 0.5;
    }

    vec3 normal = texture(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
    normal = aVertexTBN_ * normal;

    const float shadow_softness = 4.0 / 4096.0;

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    float lambert = max(dot(normal, sun_dir), 0.0);
    float visibility = 1.0;
    if (lambert > 0.00001) {
        float bias = 0.001 * tan(acos(lambert));//max(0.00125 * (1.0 - lambert), 0.00025);
        bias = clamp(bias, 0.00025, 0.002);

        float frag_depth = gl_FragCoord.z / gl_FragCoord.w;
        if (frag_depth < 8.0) {
            for (int i = 0; i < 16; i++) {
                float frag_z_ls = texture(shadow_texture, frag_pos_ls[0].xy + poisson_disk[i] * shadow_softness).r;
                if (frag_pos_ls[0].z - bias > frag_z_ls) {
                    visibility -= 1.0/16.0;
                }
            }
        } else if (frag_depth < 24.0) {
            frag_pos_ls[1].x += 0.5;
            for (int i = 0; i < 8; i++) {
                float frag_z_ls = texture(shadow_texture, frag_pos_ls[1].xy + poisson_disk[i] * shadow_softness * 0.25).r;
                if (frag_pos_ls[1].z - bias * 2.0 > frag_z_ls) {
                    visibility -= 1.0/8.0;
                }
            }
        } else if (frag_depth < 56.0) {
            frag_pos_ls[2].y += 0.5;
            for (int i = 0; i < 4; i++) {
                float frag_z_ls = texture(shadow_texture, frag_pos_ls[2].xy + poisson_disk[i] * shadow_softness * 0.125).r;
                if (frag_pos_ls[2].z - bias * 4.0 > frag_z_ls) {
                    visibility -= 1.0/4.0;
                }
            }
        } else if (frag_depth < 120.0) {
            frag_pos_ls[3].xy += 0.5;
            float frag_z_ls = texture(shadow_texture, frag_pos_ls[3].xy).r;
            if (frag_pos_ls[3].z - bias * 8.0 > frag_z_ls) {
                visibility -= 1.0;
            }
        } else {
            // use directional lightmap
            visibility = 0.0;
            additional_light = texture(lm_direct_texture, vec2(aVertexUVs2_.x, 1.0 - aVertexUVs2_.y)).rgb;
        }
    }
    
    vec3 indirect_col = texture(lm_indirect_texture, vec2(aVertexUVs2_.x, 1.0 - aVertexUVs2_.y)).rgb;
    vec3 diffuse_color = pow(texture(diffuse_texture, aVertexUVs1_).rgb, vec3(gamma)) * (sun_col * lambert * visibility + indirect_col + additional_light);
    
    outColor = vec4(diffuse_color, 1.0);
}
