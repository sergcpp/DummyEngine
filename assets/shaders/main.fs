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
    lm_indirect_sh_texture[0] : 8
    sun_dir : 10
    sun_col : 11
    gamma : 12
    
*/

uniform sampler2D diffuse_texture;
uniform sampler2D normals_texture;
uniform sampler2D shadow_texture;
uniform sampler2D lm_direct_texture;
uniform sampler2D lm_indirect_texture;
uniform sampler2D lm_indirect_sh_texture[4];

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
    const vec2 poisson_disk[32] = vec2[32](
        vec2(-0.5, 0.0),
        vec2(0.0, 0.5),
        vec2(0.5, 0.0),
        vec2(0.0, -0.5),
    
        vec2(0.0, 0.0),
        vec2(-0.1, -0.32),
        vec2(0.17, 0.31),
        vec2(0.35, 0.04),
        
        vec2(0.07, 0.7),
        vec2(-0.72, 0.09),
        vec2(0.73, 0.05),
        vec2(0.1, -0.71),
        
        vec2(0.72, 0.8),
        vec2(-0.75, 0.74),
        vec2(-0.8, -0.73),
        vec2(0.75, -0.81),
        
        vec2(-0.26, 0.75),
        vec2(0.79, 0.36),
        vec2(0.79, -0.42),
        vec2(-0.36, -0.76),
        
        vec2(-0.83, -0.34),
        vec2(0.43, -0.9),
        vec2(0.35, 0.82),
        vec2(-0.8, 0.4),
        
        vec2(-0.42, 0.18),
        vec2(0.23, 0.36),
        vec2(0.3, -0.33),
        vec2(-0.41, -0.27),
        
        vec2(-0.28, 0.97),
        vec2(0.95, -0.19),
        vec2(-0.11, -0.94),
        vec2(-0.99, 0.1)
    );

    vec3 frag_pos_ls[4];
    for (int i = 0; i < 4; i++) {
        frag_pos_ls[i] = 0.5 * aVertexShUVs_[i].xyz + 0.5;
        frag_pos_ls[i].xy *= 0.5;
    }

    vec3 normal = texture(normals_texture, aVertexUVs1_).xyz * 2.0 - 1.0;
    normal = aVertexTBN_ * normal;
    
    vec2 lm_uvs = vec2(aVertexUVs2_.x, 1.0 - aVertexUVs2_.y);

    const float shadow_softness = 2.0 / 4096.0;

    vec3 additional_light = vec3(0.0, 0.0, 0.0);

    float lambert = max(dot(normal, sun_dir), 0.0);
    float visibility = 1.0;
    if (lambert > 0.00001) {
        float bias = 0.0;//0.001 * tan(acos(lambert));//max(0.00125 * (1.0 - lambert), 0.00025);
        //bias = clamp(bias, 0.00025, 0.002);

        float frag_depth = gl_FragCoord.z / gl_FragCoord.w;
        if (frag_depth < 8.0) {
            for (int i = 0; i < 32; i++) {
                float frag_z_ls = texture(shadow_texture, frag_pos_ls[0].xy + poisson_disk[i] * shadow_softness).r;
                if (frag_pos_ls[0].z - bias > frag_z_ls) {
                    visibility -= 1.0/32.0;
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
            additional_light = texture(lm_direct_texture, lm_uvs).rgb;
        }
    }
    
    
    
    vec3 indirect_col = texture(lm_indirect_texture, lm_uvs).rgb;
    
    vec3 sh_l_00 = texture(lm_indirect_sh_texture[0], lm_uvs).rgb;
    vec3 sh_l_10 = texture(lm_indirect_sh_texture[1], lm_uvs).rgb;
    vec3 sh_l_11 = texture(lm_indirect_sh_texture[2], lm_uvs).rgb;
    vec3 sh_l_12 = texture(lm_indirect_sh_texture[3], lm_uvs).rgb;
    
    indirect_col += sh_l_00 + sh_l_10 * normal.y + sh_l_11 * normal.z + sh_l_12 * normal.x;
    
    vec3 diffuse_color = pow(texture(diffuse_texture, aVertexUVs1_).rgb, vec3(gamma)) * (sun_col * lambert * visibility + indirect_col + additional_light);
    
    outColor = vec4(diffuse_color, 1.0);
}
