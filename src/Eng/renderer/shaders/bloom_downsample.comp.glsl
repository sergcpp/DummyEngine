#version 430 core

#include "_cs_common.glsl"
#include "bloom_interface.h"

#pragma multi_compile _ COMPRESSED
#pragma multi_compile _ TONEMAP

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
#ifdef COMPRESSED
    c = decompress_hdr(c);
#endif
#if defined(TONEMAP)
    c *= texelFetch(g_exposure, ivec2(0), 0).x;
    c = limit_intensity(c, 1024.0);
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(vec3 c) {
#if defined(TONEMAP)
    c = c / (vec3(1.0) - c);
    c /= texelFetch(g_exposure, ivec2(0), 0).x;
#endif
#ifdef COMPRESSED
    c = compress_hdr(c);
#endif
    return c;
}

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

// Taken from "Next Generation Post Processing in Call of Duty Advanced Warfare"
void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 pix_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(g_params.img_size);
    const vec3 c = Tonemap(textureLod(g_input_tex, pix_uv, 0.0).xyz);

    const vec3 r0 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, +1)).xyz);
    const vec3 r1 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, -1)).xyz);
    const vec3 r2 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, -1)).xyz);
    const vec3 r3 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, +1)).xyz);

    const vec3 y0 = c;
    const vec3 y1 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-2, +0)).xyz);
    const vec3 y2 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-2, +2)).xyz);
    const vec3 y3 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, +2)).xyz);

    const vec3 g0 = c;
    const vec3 g1 = y3;
    const vec3 g2 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+2, +2)).xyz);
    const vec3 g3 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+2, +0)).xyz);

    const vec3 b0 = c;
    const vec3 b1 = g3;
    const vec3 b2 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+2, -2)).xyz);
    const vec3 b3 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, -2)).xyz);

    const vec3 p0 = c;
    const vec3 p1 = b3;
    const vec3 p2 = Tonemap(textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-2, -2)).xyz);
    const vec3 p3 = y1;

    vec3 result = vec3(0.0);
    result += 0.5 * (r0 + r1 + r2 + r3) / 4.0;    // red
    result += 0.125 * (y0 + y1 + y2 + y3) / 4.0;  // yellow
    result += 0.125 * (g0 + g1 + g2 + g3) / 4.0;  // green
    result += 0.125 * (b0 + b1 + b2 + b3) / 4.0;  // blue
    result += 0.125 * (p0 + p1 + p2 + p3) / 4.0;  // purple
    result = TonemapInvert(result);
    result = sanitize(result);

    imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), vec4(result, 0.0));
}