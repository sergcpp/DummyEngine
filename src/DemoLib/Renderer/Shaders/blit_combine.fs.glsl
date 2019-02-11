R"(
#version 310 es
#ifdef GL_ES
	precision mediump float;
#endif

/*
UNIFORMS
    s_texture : 3
    s_blured_texture : 4
    uTexSize : 6
*/
        
uniform sampler2D s_texture;
uniform sampler2D s_blured_texture;
uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    vec3 c1 = 0.1 * texture(s_blured_texture, norm_uvs).xyz;
            
    c0 += c1;
    c0 = vec3(1.0) - exp(-c0 * exposure);
    c0 = pow(c0, vec3(1.0/gamma));

    outColor = vec4(c0, 1.0);
}
)"