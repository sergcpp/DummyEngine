#version 310 es
#extension GL_ARB_texture_multisample : enable

#ifdef GL_ES
    precision mediump float;
#endif

/*
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = 0) uniform mediump sampler2DMS s_texture;
#else
layout(binding = 0) uniform mediump sampler2D s_texture;
#endif
layout(binding = 1) uniform sampler2D s_blured_texture;
layout(location = 12) uniform float tonemap;
layout(location = 13) uniform vec2 uTexSize;
layout(location = 14) uniform float gamma;
layout(location = 15) uniform float exposure;
layout(location = 16) uniform float fade;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

vec3 Unch2Tonemap(vec3 x) {
    const highp float A = 0.22; // shoulder strength
    const highp float B = 0.30; // linear strength
    const highp float C = 0.10; // linear angle
    const highp float D = 0.20; // toe strength
    const highp float E = 0.02; // toe numerator
    const highp float F = 0.30; // toe denominator
    // E / F = toe angle
    const highp float W = 11.2; // linear white point

    return ((x * (A * x + vec3(C * B)) + vec3(D * E)) / (x * ( A * x + vec3(B)) + vec3(D * F))) - vec3(E / F);
}

#if defined(MSAA_4)
vec3 BilinearTexelFetch(mediump sampler2DMS texture, vec2 texcoord, int s) {
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
#else
vec3 BilinearTexelFetch(sampler2D texture, vec2 texcoord) {
    ivec2 coord = ivec2(floor(texcoord));

    vec3 texel00 = texelFetch(texture, coord + ivec2(0, 0), 0).rgb;
    vec3 texel10 = texelFetch(texture, coord + ivec2(1, 0), 0).rgb;
    vec3 texel11 = texelFetch(texture, coord + ivec2(1, 1), 0).rgb;
    vec3 texel01 = texelFetch(texture, coord + ivec2(0, 1), 0).rgb;
            
    vec2 sample_coord = fract(texcoord.xy);
            
    vec3 texel0 = mix(texel00, texel01, sample_coord.y);
    vec3 texel1 = mix(texel10, texel11, sample_coord.y);
            
    return mix(texel0, texel1, sample_coord.x);
}
#endif

void main() {
    vec2 uvs = aVertexUVs_ - vec2(0.5, 0.5);
    vec2 norm_uvs = uvs / uTexSize;

    vec3 col;

#if defined(MSAA_4)
    vec3 c0 = BilinearTexelFetch(s_texture, uvs, 0);
    vec3 c1 = BilinearTexelFetch(s_texture, uvs, 1);
    vec3 c2 = BilinearTexelFetch(s_texture, uvs, 2);
    vec3 c3 = BilinearTexelFetch(s_texture, uvs, 3);
    vec3 c4 = texture(s_blured_texture, norm_uvs).xyz;

    col = 0.25 * (c0 + c1 + c2 + c3) + 0.1 * c4;
#else
    col = BilinearTexelFetch(s_texture, uvs) + 0.1 * texture(s_blured_texture, norm_uvs).xyz;
#endif

    if (tonemap > 0.5) {
        col = Unch2Tonemap(exposure * col);

        const highp float W = 11.2;
        vec3 white = Unch2Tonemap(vec3(W));

        // white is vec3(0.834032297)

        vec3 inv_gamma = vec3(1.0 / gamma);

        col = pow(col / white, inv_gamma);
    }

    col = mix(col, vec3(0.0), fade);
    outColor = vec4(col, 1.0);
}

