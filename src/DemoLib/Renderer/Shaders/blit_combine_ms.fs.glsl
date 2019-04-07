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

vec3 Unch2Tonemap(vec3 x) {
    const highp float A = 0.15;
    const highp float B = 0.50;
    const highp float C = 0.10;
    const highp float D = 0.20;
    const highp float E = 0.02;
    const highp float F = 0.30;
    const highp float W = 11.2;

    return ((x * (A * x + vec3(C * B)) + vec3(D * E)) / (x * ( A * x + vec3(B)) + vec3(D * F))) - vec3(E / F);
}

vec3 BilinearTexelFetch(sampler2DMS texture, vec2 texcoord, int s) {
    ivec2 coord = ivec2(floor(texcoord));

    vec3 texel00 = texelFetch(texture, coord + ivec2(0, 0), s).rgb;
    vec3 texel10 = texelFetch(texture, coord + ivec2(1, 0), s).rgb;
    vec3 texel11 = texelFetch(texture, coord + ivec2(1, 1), s).rgb;
    vec3 texel01 = texelFetch(texture, coord + ivec2(0, 1), s).rgb;
            
    vec2 sample_coord = fract(texcoord.xy);
            
    vec3 texel0 = mix(texel00, texel01, sample_coord.y);
    vec3 texel1 = mix(texel10, texel11, sample_coord.y);
            
    return mix(texel0, texel1, sample_coord.x);
}

void main() {
    vec2 uvs = aVertexUVs_ - vec2(0.5, 0.5);
    vec2 norm_uvs = uvs / uTexSize;

    vec3 c0 = BilinearTexelFetch(s_texture, uvs, 0);
    vec3 c1 = BilinearTexelFetch(s_texture, uvs, 1);
    vec3 c2 = BilinearTexelFetch(s_texture, uvs, 2);
    vec3 c3 = BilinearTexelFetch(s_texture, uvs, 3);

    //vec3 c0 = texelFetch(s_texture, ivec2(aVertexUVs_), 0).xyz;
    //vec3 c1 = texelFetch(s_texture, ivec2(aVertexUVs_), 1).xyz;
    //vec3 c2 = texelFetch(s_texture, ivec2(aVertexUVs_), 2).xyz;
    //vec3 c3 = texelFetch(s_texture, ivec2(aVertexUVs_), 3).xyz;
    vec3 c4 = 0.1 * texture(s_blured_texture, norm_uvs).xyz;
            
    c0 += c4;
    c1 += c4;
    c2 += c4;
    c3 += c4;

    c0 = Unch2Tonemap(exposure * c0);
    c1 = Unch2Tonemap(exposure * c1);
    c2 = Unch2Tonemap(exposure * c2);
    c3 = Unch2Tonemap(exposure * c3);

    const highp float W = 11.2;
    vec3 white = 1.0 / Unch2Tonemap(vec3(W));

    vec3 inv_gamma = vec3(1.0 / gamma);

    c0 = pow(c0 * white, inv_gamma);
    c1 = pow(c1 * white, inv_gamma);
    c2 = pow(c2 * white, inv_gamma);
    c3 = pow(c3 * white, inv_gamma);

    outColor = vec4(0.25 * (c0 + c1 + c2 + c3), 1.0);
}
)"