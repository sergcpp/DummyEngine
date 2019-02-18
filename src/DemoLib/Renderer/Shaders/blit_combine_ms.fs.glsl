R"(
#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif
        
layout(binding = 0) uniform mediump sampler2DMS s_texture;
layout(binding = 1) uniform sampler2D s_blured_texture;
layout(location = 13) uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;

in vec2 aVertexUVs_;

out vec4 outColor;

void main() {
    vec2 norm_uvs = aVertexUVs_ / uTexSize;

    vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    vec3 c1 = texelFetch(s_texture, ivec2(aVertexUVs_), 1).xyz;
    vec3 c2 = texelFetch(s_texture, ivec2(aVertexUVs_), 2).xyz;
    vec3 c3 = texelFetch(s_texture, ivec2(aVertexUVs_), 3).xyz;
    vec3 c4 = 0.1 * texture(s_blured_texture, norm_uvs).xyz;
            
    c0 += c4;
    c1 += c4;
    c2 += c4;
    c3 += c4;

    //c0 = exposure * c0 / (c0 + vec3(1.0));
    //c1 = exposure * c1 / (c1 + vec3(1.0));
    //c2 = exposure * c2 / (c2 + vec3(1.0));
    //c3 = exposure * c3 / (c3 + vec3(1.0));

    c0 = vec3(1.0) - exp(-c0 * exposure);
    c1 = vec3(1.0) - exp(-c1 * exposure);
    c2 = vec3(1.0) - exp(-c2 * exposure);
    c3 = vec3(1.0) - exp(-c3 * exposure);

    c0 = pow(c0, vec3(1.0/gamma));
    c1 = pow(c1, vec3(1.0/gamma));
    c2 = pow(c2, vec3(1.0/gamma));
    c3 = pow(c3, vec3(1.0/gamma));

    outColor = vec4(0.25 * (c0 + c1 + c2 + c3), 1.0);
}
)"