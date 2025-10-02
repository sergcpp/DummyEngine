#version 430 core

#include "_cs_common.glsl"
#include "sharpen_interface.h"

#pragma multi_compile _ COMPRESSED

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = INPUT_TEX_SLOT) uniform sampler2D g_input_tex;
layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exposure;

#ifdef COMPRESSED
    layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_out_img;
#else
    layout(binding = OUT_IMG_SLOT, rgba32f) uniform image2D g_out_img;
#endif

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 Tonemap(vec3 c) {
#if 1//defined(TONEMAP)
    c *= (texelFetch(g_exposure, ivec2(0), 0).x / g_params.pre_exposure);
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(vec3 c) {
#if 1//defined(TONEMAP)
    c = c / (vec3(1.0) - c);
    c /= (texelFetch(g_exposure, ivec2(0), 0).x / g_params.pre_exposure);
#endif
    return c;
}

layout(local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 pix_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(g_params.img_size);

    // Sharpening filter taken from https://github.com/GPUOpen-Effects/FidelityFX-CAS

    // a b c
    // d e f
    // g h i
    const vec3 a = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, -1)).xyz);
    const vec3 b = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, -1)).xyz);
    const vec3 c = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, -1)).xyz);
    const vec3 d = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1,  0)).xyz);
    const vec3 e = Tonemap(textureLod(g_input_tex, pix_uv, 0.0).xyz);
    const vec3 f = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, +0)).xyz);
    const vec3 g = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, +1)).xyz);
    const vec3 h = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, +1)).xyz);
    const vec3 i = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, +1)).xyz);

    // Soft min and max
    //  a b c             b
    //  d e f * 0.5  +  d e f * 0.5
    //  g h i             h
    // These are 2.0x bigger (factored out the extra multiply)
    float mnR = min3(min3(d.r, e.r, f.r), b.r, h.r);
    float mnG = min3(min3(d.g, e.g, f.g), b.g, h.g);
    float mnB = min3(min3(d.b, e.b, f.b), b.b, h.b);
    { // better diagonals branch
        mnR += min3(min3(mnR, a.r, c.r), g.r, i.r);
        mnG += min3(min3(mnG, a.g, c.g), g.g, i.g);
        mnB += min3(min3(mnB, a.b, c.b), g.b, i.b);
    }
    float mxR = max3(max3(d.r, e.r, f.r), b.r, h.r);
    float mxG = max3(max3(d.g, e.g, f.g), b.g, h.g);
    float mxB = max3(max3(d.b, e.b, f.b), b.b, h.b);
    { // better diagonals branch
        mxR += max3(max3(mxR, a.r, c.r), g.r, i.r);
        mxG += max3(max3(mxG, a.g, c.g), g.g, i.g);
        mxB += max3(max3(mxB, a.b, c.b), g.b, i.b);
    }
    // Smooth minimum distance to signal limit divided by smooth max
    float ampR = saturate(min(mnR, 2.0 - mxR) / mxR);
    float ampG = saturate(min(mnG, 2.0 - mxG) / mxG);
    float ampB = saturate(min(mnB, 2.0 - mxB) / mxB);
    // Shaping amount of sharpening
    ampR = sqrt(ampR);
    ampG = sqrt(ampG);
    ampB = sqrt(ampB);

    // Filter shape
    //  0 w 0
    //  w 1 w
    //  0 w 0
    const float peak = -1.0 / mix(8.0, 5.0, g_params.sharpness);
    const float wR = ampR * peak;
    const float wG = ampG * peak;
    const float wB = ampB * peak;
    // Filter
    vec3 result;
    result.r = saturate((b.r * wR + d.r * wR + f.r * wR + h.r * wR + e.r) / (1.0 + 4.0 * wR));
    result.g = saturate((b.g * wG + d.g * wG + f.g * wG + h.g * wG + e.g) / (1.0 + 4.0 * wG));
    result.b = saturate((b.b * wB + d.b * wB + f.b * wB + h.b * wB + e.b) / (1.0 + 4.0 * wB));

    result = TonemapInvert(result);
    result = sanitize(result);

    imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), vec4(result, 0.0));
}