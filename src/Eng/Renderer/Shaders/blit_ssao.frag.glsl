R"(#version 310 es
#extension GL_EXT_texture_buffer : enable

#ifdef GL_ES
    precision mediump float;
#endif

/*
UNIFORM_BLOCKS
    SharedDataBlock : )" AS_STR(REN_UB_SHARED_DATA_LOC) R"(
*/

)"
#include "_fs_common.glsl"
R"(

layout (std140) uniform SharedDataBlock {
    SharedData shrd_data;
};

layout(binding = REN_BASE0_TEX_SLOT) uniform mediump sampler2D depth_texture;
layout(binding = REN_BASE1_TEX_SLOT) uniform mediump sampler2D rand_texture;
layout(binding = REN_BASE2_TEX_SLOT) uniform lowp sampler2D cone_rt_lut;

layout(binding = REN_CELLS_BUF_SLOT) uniform highp usamplerBuffer cells_buffer;
layout(binding = REN_ITEMS_BUF_SLOT) uniform highp usamplerBuffer items_buffer;

in vec2 aVertexUVs_;

out vec4 outColor;

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

    ///////////////////

    {
        highp float k = log2(lin_depth / shrd_data.uClipInfo[1]) / shrd_data.uClipInfo[3];
        int slice = int(floor(k * float(REN_GRID_RES_Z)));
    
        int ix = 2 * icoords.x, iy = 2 * icoords.y;
        int cell_index = slice * REN_GRID_RES_X * REN_GRID_RES_Y + (iy * REN_GRID_RES_Y / int(shrd_data.uResAndFRes.y)) * REN_GRID_RES_X + ix * REN_GRID_RES_X / int(shrd_data.uResAndFRes.x);

        highp uvec2 cell_data = texelFetch(cells_buffer, cell_index).xy;
        highp uvec2 offset_and_ecount = uvec2(bitfieldExtract(cell_data.x, 0, 24), bitfieldExtract(cell_data.y, 16, 8));

        vec2 norm_uvs = 2.0 * aVertexUVs_.xy / shrd_data.uResAndFRes.xy;

        float depth = DelinearizeDepth(lin_depth, shrd_data.uClipInfo);
        vec4 cone_origin_cs = vec4(norm_uvs, depth, 1.0);
        cone_origin_cs.xyz = 2.0 * cone_origin_cs.xyz - vec3(1.0);

        vec4 cone_origin_ws = shrd_data.uInvViewProjMatrix * cone_origin_cs;
        cone_origin_ws /= cone_origin_ws.w;

        vec3 cone_dir_ws = normalize(vec3(-2.0, 1.0, 0.0));

        for (uint i = offset_and_ecount.x; i < offset_and_ecount.x + offset_and_ecount.y; i++) {
            highp uint item_data = texelFetch(items_buffer, int(i)).y;
            int ei = int(bitfieldExtract(item_data, 0, 8));

            vec4 pos_and_radius = shrd_data.uEllipsoids[ei].pos_and_radius;
            vec4 axis_and_perp = shrd_data.uEllipsoids[ei].axis_and_perp;

            mat3 sph_ls;
            sph_ls[0] = vec3(0.0);
            sph_ls[0][floatBitsToInt(axis_and_perp.w)] = 1.0;
            sph_ls[1] = axis_and_perp.xyz;
            sph_ls[2] = normalize(cross(sph_ls[0], sph_ls[1]));
            sph_ls = transpose(sph_ls);

            vec3 cone_origin_ls = sph_ls * (cone_origin_ws.xyz - pos_and_radius.xyz);
            vec3 cone_dir_ls = sph_ls * cone_dir_ws;

            vec3 dir = -cone_origin_ls;
            float dist = length(dir);

            float sin_omega = pos_and_radius.w / sqrt(pos_and_radius.w * pos_and_radius.w + dist * dist);
            float cos_phi = dot(dir, cone_dir_ls) / dist;

            //occlusion *= textureLod(cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).g;
            //occlusion *= 0.0;
        }

#if 0
        const vec3 sph_pos[2] = vec3[](vec3(4.0, 1.0, 0.0), vec3(5.0, 3.0, 0.0));
        const vec3 sph_axis[2] = vec3[](vec3(0.0, 1.0, 0.0), vec3(0.0, 1.4, 1.4));
        const vec3 sph_perp[2] = vec3[](vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0));
        const float sph_radius = 1.0;

        for (int i = 0; i < 2; i++) {
            mat3 sph_ls = mat3(sph_perp[i], sph_axis[i], normalize(cross(sph_perp[i], sph_axis[i])));
            sph_ls = transpose(sph_ls);

            vec3 cone_origin_ls = sph_ls * (cone_origin_ws.xyz - sph_pos[i]);
            vec3 cone_dir_ls = sph_ls * cone_dir_ws;

            vec3 dir = /*sph_pos[i]*/ - cone_origin_ls;
            float dist = length(dir);

            float sin_omega = sph_radius / sqrt(sph_radius * sph_radius + dist * dist);
            float cos_phi = dot(dir, cone_dir_ls) / dist;

            occlusion *= textureLod(cone_rt_lut, vec2(cos_phi, sin_omega), 0.0).g;
        }
#endif
    }
    ///////////////////

    outColor = vec4(occlusion, occlusion, occlusion, 1.0);
}
)"