#version 320 es
#extension GL_ARB_texture_multisample : enable

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_combine_interface.h"

/*
PERM @LUT
*/

layout(binding = HDR_TEX_SLOT) uniform mediump sampler2D g_tex;
layout(binding = BLURED_TEX_SLOT) uniform sampler2D g_blured_tex;
#if defined(LUT)
layout(binding = LUT_TEX_SLOT) uniform sampler3D g_lut_tex;
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;

vec3 TonemapLUT(sampler3D lut, vec3 col) {
    const vec3 encoded = col / (col + 1.0);

    // Align the encoded range to texel centers
    const float LUT_DIMS = 48.0;
    const vec3 uv = encoded * (LUT_DIMS - 1.0) / LUT_DIMS;

    return textureLod(lut, uv, 0.0).xyz;
}

void main() {
    vec3 col = textureLod(g_tex, g_vtx_uvs, 0.0).rgb + 0.1 * textureLod(g_blured_tex, g_vtx_uvs, 0.0).xyz;
    col *= g_params.exposure;

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
    g_out_color = vec4(col + dither / 255.0, 1.0);
}

