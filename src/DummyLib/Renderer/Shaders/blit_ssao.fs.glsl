R"(
#version 310 es

#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

struct ShadowMapRegion {
    vec4 transform;
    mat4 clip_from_world;
};

layout (std140) uniform SharedDataBlock {
    mat4 uViewMatrix, uProjMatrix, uViewProjMatrix;
    mat4 uInvViewMatrix, uInvProjMatrix, uInvViewProjMatrix, uDeltaMatrix;
    ShadowMapRegion uShadowMapRegions[)" AS_STR(REN_MAX_SHADOWMAPS_TOTAL) R"(];
    vec4 uSunDir, uSunCol;
    vec4 uClipInfo, uCamPosAndGamma;
    vec4 uResAndFRes;
};

layout(binding = )" AS_STR(REN_DIFF_TEX_SLOT) R"() uniform mediump sampler2D depth_texture;

in vec2 aVertexUVs_;

out vec4 outColor;

float SampleDepthTexel(vec2 texcoord) {
    ivec2 coord = ivec2(texcoord);
    return texelFetch(depth_texture, coord, 0).r;
}

void main() {
    const mediump vec2 _transforms_4x4[16] = vec2[16](
        vec2(0.0,     1.0),     // 4
        vec2(0.9238,  0.3826),  // 1
        vec2(0.3826,  0.9238),  // 3
        vec2(-0.9238, 0.3826),  // 7
        vec2(-1.0,    0.0),     // 8
        vec2(-0.3826, -0.9238), // 11
        vec2(0.9238,  -0.3826), // 15
        vec2(-0.7071, 0.7071),  // 6
        vec2(-0.7071, -0.7071), // 10
        vec2(0.7071,  0.7071),  // 2
        vec2(0.7071,  -0.7071), // 14
        vec2(-0.3826, 0.9238),  // 5
        vec2(1.0,     0.0),     // 0
        vec2(0.3826,  -0.9238), // 13
        vec2(0.0,     -1.0),    // 12
        vec2(-0.9238, -0.3826)  // 9
    );

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

    float depth = SampleDepthTexel(aVertexUVs_);
    if (depth > fadeout_end) {
        outColor = vec4(1.0, 1.0, 1.0, 1.0);
        return;
    }

    float initial_radius = 0.15;
    float ss_radius = min(initial_radius / depth, 0.1);

    const float sample_weight = 1.0 / 7.0;
    float occlusion = 0.5 * sample_weight;

    for (int i = 0; i < 3; i++) {
        int c = 4 * (int(gl_FragCoord.y) % 4) + (int(gl_FragCoord.x) % 4);
        mat2 transform = mat2(_transforms_4x4[c], vec2(-_transforms_4x4[c].y, _transforms_4x4[c].x));
        vec2 sample_point = transform * sample_points[i];

        vec2 coord_offset = 0.5 * ss_radius * sample_point * uResAndFRes.xy;

        vec2 depth_values = vec2(SampleDepthTexel(aVertexUVs_ + coord_offset),
                                 SampleDepthTexel(aVertexUVs_ - coord_offset));
        float sphere_width = initial_radius * sphere_widths[i];

        vec2 depth_diff = vec2(depth) - depth_values;

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
    float k = max((depth - fadeout_start) / (fadeout_end - fadeout_start), 0.0);
    occlusion = mix(occlusion, 1.0, k);

    outColor = vec4(occlusion, occlusion, occlusion, 1.0);
}
)"