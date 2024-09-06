#version 430 core
#extension GL_ARB_texture_multisample : enable

#include "_fs_common.glsl"
#include "blit_postprocess_interface.h"

#pragma multi_compile _ COMPRESSED
#pragma multi_compile _ ABERRATION
#pragma multi_compile _ LUT
#pragma multi_compile _ TWO_TARGETS

layout(binding = HDR_TEX_SLOT) uniform sampler2D g_tex;
layout(binding = BLURED_TEX_SLOT) uniform sampler2D g_blured_tex;
layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exp_tex;
#if defined(LUT)
    layout(binding = LUT_TEX_SLOT) uniform sampler3D g_lut_tex;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;
#ifdef TWO_TARGETS
    layout(location = 1) out vec4 g_out_color2;
#endif

vec3 TonemapLUT(sampler3D lut, vec3 col) {
    const vec3 encoded = col / (col + 1.0);

    // Align the encoded range to texel centers
    const float LUT_DIMS = 48.0;
    const vec3 uv = encoded * (LUT_DIMS - 1.0) / LUT_DIMS;

    return textureLod(lut, uv, 0.0).xyz;
}

void main() {
    vec3 col = vec3(0.0);
#ifdef ABERRATION
    if (g_params.aberration > 0.0) {
        vec2 uvs_centered = g_vtx_uvs - 0.5;
        const vec3 AberrFactors[5] = vec3[](vec3(0.4, 0.05, 0.0),
                                            vec3(0.3, 0.2, 0.1),
                                            vec3(0.2, 0.5, 0.2),
                                            vec3(0.1, 0.2, 0.3),
                                            vec3(0.0, 0.05, 0.4));
        for (int i = -2; i <= 2; ++i) {
            col += AberrFactors[2 + i] * textureLod(g_tex, vec2(0.5) + uvs_centered * (1.0 - float(i) * 0.0005 * g_params.aberration), 0.0).xyz;
        }
    } else
#endif
    {
        col += textureLod(g_tex, g_vtx_uvs, 0.0).rgb;
    }

    // add bloom
    col += 0.1 * textureLod(g_blured_tex, g_vtx_uvs, 0.0).xyz;

#ifdef COMPRESSED
    col = decompress_hdr(col);
#endif
    col *= texelFetch(g_exp_tex, ivec2(0), 0).x;

#if defined(LUT)
    col = TonemapLUT(g_lut_tex, col);
#else
    if (g_params.tonemap_mode > 0.5) {
        col = LinearToSRGB(col);
    }
#endif

    col = mix(col, vec3(0.0), g_params.fade);

    if (g_params.inv_gamma != 1.0) {
        col = pow(col, vec3(g_params.inv_gamma));
    }

    const vec3 dither = vec3(Bayer4x4(uvec2(gl_FragCoord.xy), 0),
                             Bayer4x4(uvec2(gl_FragCoord.xy), 1),
                             Bayer4x4(uvec2(gl_FragCoord.xy), 2));
    const vec4 result = vec4(col + dither / 255.0, 1.0);

    g_out_color = result;
#ifdef TWO_TARGETS
    g_out_color2 = result;
#endif
}

