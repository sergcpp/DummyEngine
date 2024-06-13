#version 320 es

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "blit_ssao_interface.h"

layout(binding = DEPTH_TEX_SLOT) uniform mediump sampler2D g_depth_tex;
layout(binding = RAND_TEX_SLOT) uniform mediump sampler2D g_rand_tex;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

#if defined(VULKAN)
layout(location = 0) in vec2 g_vtx_uvs;
#else
in vec2 g_vtx_uvs;
#endif

layout(location = 0) out vec4 g_out_color;

float SampleDepthTexel(vec2 texcoord) {
    ivec2 coord = ivec2(texcoord);
    return texelFetch(g_depth_tex, coord, 0).r;
}

void main() {
    const vec2 sample_points[3] = vec2[3](
        vec2(-0.0625, 0.1082),  // 1.0/8.0
        vec2(-0.1875, -0.3247), // 3.0/8.0
        vec2(0.75, 0.0)         // 6.0/8.0
    );

    const float sphere_widths[3] = float[3](
        0.99215, 0.92702, 0.66144
    );

    const float fadeout_start = 16.0;
    const float fadeout_end = 64.0;

    float lin_depth = SampleDepthTexel(g_vtx_uvs);
    if (lin_depth > fadeout_end) {
        g_out_color = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    float initial_radius = 0.3;
    float ss_radius = min(initial_radius / lin_depth, 0.1);

    const float sample_weight = 1.0 / 7.0;
    float occlusion = 0.5 * sample_weight;

    ivec2 icoords = ivec2(gl_FragCoord.xy);
    ivec2 rcoords = icoords % ivec2(4);

    for (int i = 0; i < 3; i++) {
        mat2 transform;
        transform[0] = texelFetch(g_rand_tex, rcoords, 0).xy;
        transform[1] = vec2(-transform[0].y, transform[0].x);

        vec2 sample_point = transform * sample_points[i];

        vec2 coord_offset = 0.5 * ss_radius * sample_point * g_params.resolution;

        vec2 depth_values = vec2(SampleDepthTexel(g_vtx_uvs + coord_offset),
                                 SampleDepthTexel(g_vtx_uvs - coord_offset));
        float sphere_width = initial_radius * sphere_widths[i];

        vec2 depth_diff = vec2(lin_depth) - depth_values;

        vec2 occ_values = clamp(depth_diff / sphere_width + vec2(0.5), vec2(0.0), vec2(1.0));

        const float max_dist = 1.0;
        vec2 dist_mod = clamp((vec2(max_dist) - depth_diff) / max_dist, vec2(0.0), vec2(1.0));

        vec2 mod_cont = mix(mix(vec2(0.5), 1.0 - occ_values.yx, dist_mod.yx), occ_values.xy, dist_mod.xy);
        mod_cont *= sample_weight;

        occlusion += mod_cont.x;
        occlusion += mod_cont.y;
    }

    occlusion = clamp(1.0 - 2.0 * (occlusion - 0.5), 0.0, 1.0);

    // smooth fadeout
    float k = max((lin_depth - fadeout_start) / (fadeout_end - fadeout_start), 0.0);
    occlusion = mix(occlusion, 1.0, k);

    g_out_color = vec4(occlusion, occlusion, occlusion, 1.0);
}
