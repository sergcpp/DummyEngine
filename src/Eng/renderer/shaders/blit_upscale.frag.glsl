#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision mediump float;
#endif

#include "_fs_common.glsl"
#include "blit_upscale_interface.h"

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
layout(binding = DEPTH_LOW_TEX_SLOT) uniform mediump sampler2D g_depth_low_tex;
layout(binding = INPUT_TEX_SLOT) uniform lowp sampler2D g_input_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;
layout(location = 0) out vec4 g_out_color;

void main() {
    vec2 norm_uvs = g_vtx_uvs;
    vec2 texel_size_low = vec2(0.5) / g_params.resolution.zw;

    float d0 = LinearizeDepth(textureLod(g_depth_tex, norm_uvs, 0.0).r, g_params.clip_info);

    float d1 = abs(d0 - textureLod(g_depth_low_tex, norm_uvs, 0.0).r);
    float d2 = abs(d0 - textureLod(g_depth_low_tex, norm_uvs + vec2(0, texel_size_low.y), 0.0).r);
    float d3 = abs(d0 - textureLod(g_depth_low_tex, norm_uvs + vec2(texel_size_low.x, 0), 0.0).r);
    float d4 = abs(d0 - textureLod(g_depth_low_tex, norm_uvs + texel_size_low, 0.0).r);

    float dmin = min(min(d1, d2), min(d3, d4));

    if (dmin < 0.05) {
        g_out_color = textureLod(g_input_tex, norm_uvs * g_params.resolution.xy / g_params.resolution.zw, 0.0);
    } else {
        if (dmin == d1) {
            g_out_color = textureLod(g_input_tex, norm_uvs, 0.0);
        }else if (dmin == d2) {
            g_out_color = textureLod(g_input_tex, norm_uvs + vec2(0, texel_size_low.y), 0.0);
        } else if (dmin == d3) {
            g_out_color = textureLod(g_input_tex, norm_uvs + vec2(texel_size_low.x, 0), 0.0);
        } else if (dmin == d4) {
            g_out_color = textureLod(g_input_tex, norm_uvs + texel_size_low, 0.0);
        }
    }
}
