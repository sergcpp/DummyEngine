#version 310 es
#extension GL_EXT_texture_buffer : enable

#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : $ubSharedDataLoc
*/

#include "_fs_common.glsl"

#if defined(VULKAN) || defined(GL_SPIRV)
layout (binding = REN_UB_SHARED_DATA_LOC, std140)
#else
layout (std140)
#endif
uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2D depth_texture;
layout(binding = REN_BASE1_TEX_SLOT) uniform mediump sampler2D rand_texture;

layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

#if defined(VULKAN) || defined(GL_SPIRV)
layout(location = 0) in vec2 aVertexUVs_;
#else
in vec2 aVertexUVs_;
#endif

layout(location = 0) out vec4 outColor;

float SampleDepthTexel(vec2 texcoord) {
    ivec2 coord = ivec2(texcoord);
    return texelFetch(depth_texture, coord, 0).r;
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

    float lin_depth = SampleDepthTexel(aVertexUVs_);
    if (lin_depth > fadeout_end) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
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
        transform[0] = texelFetch(rand_texture, rcoords, 0).xy;
        transform[1] = vec2(-transform[0].y, transform[0].x);

        vec2 sample_point = transform * sample_points[i];

        vec2 coord_offset = 0.5 * ss_radius * sample_point * shrd_data.uResAndFRes.xy;

        vec2 depth_values = vec2(SampleDepthTexel(aVertexUVs_ + coord_offset),
                                 SampleDepthTexel(aVertexUVs_ - coord_offset));
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

    outColor = vec4(occlusion, occlusion, occlusion, 1.0);
}
