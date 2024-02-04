#version 310 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_combine_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
PERM @MSAA_4
*/

#if defined(MSAA_4)
layout(binding = HDR_TEX_SLOT) uniform mediump sampler2DMS g_tex;
#else
layout(binding = HDR_TEX_SLOT) uniform mediump sampler2D g_tex;
#endif
layout(binding = BLURED_TEX_SLOT) uniform sampler2D g_blured_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

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

    vec2 sample_coord = fract(texcoord);

    vec3 texel0 = mix(texel00, texel01, sample_coord.y);
    vec3 texel1 = mix(texel10, texel11, sample_coord.y);

    return mix(texel0, texel1, sample_coord.x);
}
#endif

vec3 to_srgb(vec3 linear_rgb) {
    bvec3 cutoff = lessThan(linear_rgb, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linear_rgb.rgb, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linear_rgb * vec3(12.92);

    return mix(higher, lower, cutoff);
}

void main() {
    vec2 uvs = g_vtx_uvs - vec2(0.5, 0.5);
    vec2 norm_uvs = uvs / g_params.tex_size;

    vec3 col;

#if defined(MSAA_4)
    vec3 c0 = BilinearTexelFetch(g_tex, uvs, 0);
    vec3 c1 = BilinearTexelFetch(g_tex, uvs, 1);
    vec3 c2 = BilinearTexelFetch(g_tex, uvs, 2);
    vec3 c3 = BilinearTexelFetch(g_tex, uvs, 3);
    vec3 c4 = texture(g_blured_tex, norm_uvs).xyz;

    col = 0.25 * (c0 + c1 + c2 + c3) + 0.1 * c4;
#else
    col = BilinearTexelFetch(g_tex, uvs) + 0.1 * texture(g_blured_tex, norm_uvs).xyz;
#endif

    if (g_params.tonemap > 0.5) {
        col = Unch2Tonemap(g_params.exposure * col);

        const highp float W = 11.2;
        vec3 white = Unch2Tonemap(vec3(W));

        // white is vec3(0.834032297)

        //vec3 inv_gamma = vec3(1.0 / g_params.gamma);
        //col = pow(col / white, inv_gamma);

        col /= white;
    }

#if defined(VULKAN)
    col = to_srgb(col);
#endif

    col = mix(col, vec3(0.0), g_params.fade);
    g_out_color = vec4(col, 1.0);
}

