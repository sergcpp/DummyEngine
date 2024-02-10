#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision mediump int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "taa_common.glsl"
#include "blit_taa_interface.h"

/*
PERM @USE_CLIPPING
PERM @USE_ROUNDED_NEIBOURHOOD
PERM @USE_TONEMAP
PERM @USE_YCoCg
PERM @USE_CLIPPING;USE_TONEMAP
PERM @USE_CLIPPING;USE_TONEMAP;USE_YCoCg
PERM @USE_CLIPPING;USE_ROUNDED_NEIBOURHOOD
PERM @USE_CLIPPING;USE_ROUNDED_NEIBOURHOOD;USE_TONEMAP;USE_YCoCg
PERM @USE_STATIC_ACCUMULATION
*/

layout(binding = CURR_TEX_SLOT) uniform mediump sampler2D g_color_curr;
layout(binding = HIST_TEX_SLOT) uniform mediump sampler2D g_color_hist;

layout(binding = DEPTH_TEX_SLOT) uniform mediump sampler2D g_depth;
layout(binding = VELOCITY_TEX_SLOT) uniform mediump sampler2D g_velocity;

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

LAYOUT(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out vec3 g_out_color;
layout(location = 1) out vec3 g_out_history;

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 Tonemap(in vec3 c) {
#if defined(USE_TONEMAP)
    c *= g_params.exposure;
    return c * rcp(max3(c.r, c.g, c.b) + 1.0);
#else
    return c;
#endif
}

vec3 TonemapInvert(in vec3 c) {
#if defined(USE_TONEMAP)
    return (c / max(g_params.exposure, 0.001)) * rcp(1.0 - max3(c.r, c.g, c.b));
#else
    return c;
#endif
}

vec3 FetchColor(sampler2D s, ivec2 icoord) {
#if defined(USE_YCoCg)
    return RGB_to_YCoCg(Tonemap(texelFetch(s, icoord, 0).rgb));
#else
    return Tonemap(texelFetch(s, icoord, 0).rgb);
#endif
}

vec3 SampleColor(sampler2D s, vec2 uvs) {
#if defined(USE_YCoCg)
    return RGB_to_YCoCg(Tonemap(textureLod(s, uvs, 0.0).rgb));
#else
    return Tonemap(textureLod(s, uvs, 0.0).rgb);
#endif
}

float Luma(vec3 col) {
#if defined(USE_YCoCg)
    return col.r;
#else
    return dot(col, vec3(0.2125, 0.7154, 0.0721));
#endif
}

vec3 find_closest_fragment_3x3(sampler2D dtex, vec2 uv, vec2 texel_size) {
    vec2 du = vec2(texel_size.x, 0.0);
    vec2 dv = vec2(0.0, texel_size.y);

    vec3 dtl = vec3(-1, -1, textureLod(dtex, uv - dv - du, 0.0).x);
    vec3 dtc = vec3( 0, -1, textureLod(dtex, uv - dv, 0.0).x);
    vec3 dtr = vec3( 1, -1, textureLod(dtex, uv - dv + du, 0.0).x);

    vec3 dml = vec3(-1, 0, textureLod(dtex, uv - du, 0.0).x);
    vec3 dmc = vec3( 0, 0, textureLod(dtex, uv, 0.0).x);
    vec3 dmr = vec3( 1, 0, textureLod(dtex, uv + du, 0.0).x);

    vec3 dbl = vec3(-1, 1, textureLod(dtex, uv + dv - du, 0.0).x);
    vec3 dbc = vec3( 0, 1, textureLod(dtex, uv + dv, 0.0).x);
    vec3 dbr = vec3( 1, 1, textureLod(dtex, uv + dv + du, 0.0).x);

    vec3 dmin = dtl;
    if (dmin.z > dtc.z) dmin = dtc;
    if (dmin.z > dtr.z) dmin = dtr;

    if (dmin.z > dml.z) dmin = dml;
    if (dmin.z > dmc.z) dmin = dmc;
    if (dmin.z > dmr.z) dmin = dmr;

    if (dmin.z > dbl.z) dmin = dbl;
    if (dmin.z > dbc.z) dmin = dbc;
    if (dmin.z > dbr.z) dmin = dbr;

    return vec3(uv + texel_size * dmin.xy, dmin.z);
}

void main() {
    ivec2 uvs_px = ivec2(g_vtx_uvs);
    vec2 texel_size = vec2(1.0) / g_params.tex_size;
    vec2 norm_uvs = g_vtx_uvs / g_params.tex_size;

    vec3 col_curr = FetchColor(g_color_curr, uvs_px);

#if defined(USE_STATIC_ACCUMULATION)
    vec3 col_hist = FetchColor(g_color_hist, uvs_px);
    vec3 col = mix(col_hist, col_curr, g_params.mix_factor);

    g_out_color = TonemapInvert(col);
    g_out_history = g_out_color;
#else // USE_STATIC_ACCUMULATION
    float min_depth = texelFetch(g_depth, uvs_px, 0).r;

    const ivec2 offsets[8] = ivec2[8](
        ivec2(-1, 1),   ivec2(0, 1),    ivec2(1, 1),
        ivec2(-1, 0),                   ivec2(1, 0),
        ivec2(-1, -1),  ivec2(1, -1),   ivec2(1, -1)
    );

#if !defined(USE_YCoCg) && !defined(USE_ROUNDED_NEIBOURHOOD)
    vec3 col_avg = col_curr, col_var = col_curr * col_curr;
    ivec2 closest_frag = ivec2(0, 0);

    for (int i = 0; i < 8; i++) {
        float depth = texelFetch(g_depth, uvs_px + offsets[i], 0).r;
        if (depth < min_depth) {
            closest_frag = offsets[i];
            min_depth = depth;
        }

        vec3 col = FetchColor(g_color_curr, uvs_px + offsets[i]);
        col_avg += col;
        col_var += col * col;
    }

    col_avg /= 9.0;
    col_var /= 9.0;

    vec3 sigma = sqrt(max(col_var - col_avg * col_avg, vec3(0.0)));
    vec3 col_min = col_avg - 1.25 * sigma;
    vec3 col_max = col_avg + 1.25 * sigma;

    vec2 closest_vel = texelFetch(g_velocity, clamp(uvs_px + closest_frag, ivec2(0), ivec2(g_params.tex_size - vec2(1))), 0).rg;
#else
    vec3 col_tl = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, -texel_size.y));
    vec3 col_tc = SampleColor(g_color_curr, norm_uvs + vec2(0, -texel_size.y));
    vec3 col_tr = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, -texel_size.y));
    vec3 col_ml = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, 0));
    vec3 col_mc = col_curr;
    vec3 col_mr = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, 0));
    vec3 col_bl = SampleColor(g_color_curr, norm_uvs + vec2(-texel_size.x, texel_size.y));
    vec3 col_bc = SampleColor(g_color_curr, norm_uvs + vec2(0, texel_size.y));
    vec3 col_br = SampleColor(g_color_curr, norm_uvs + vec2(texel_size.x, texel_size.y));

    vec3 col_min = min3(min3(col_tl, col_tc, col_tr),
                        min3(col_ml, col_mc, col_mr),
                        min3(col_bl, col_bc, col_br));
    vec3 col_max = max3(max3(col_tl, col_tc, col_tr),
                        max3(col_ml, col_mc, col_mr),
                        max3(col_bl, col_bc, col_br));

    vec3 col_avg = (col_tl + col_tc + col_tr + col_ml + col_mc + col_mr + col_bl + col_bc + col_br) / 9.0;

    #if defined(USE_ROUNDED_NEIBOURHOOD)
        vec3 col_min5 = min(col_tc, min(col_ml, min(col_mc, min(col_mr, col_bc))));
        vec3 col_max5 = max(col_tc, max(col_ml, max(col_mc, max(col_mr, col_bc))));
        vec3 col_avg5 = (col_tc + col_ml + col_mc + col_mr + col_bc) / 5.0;
        col_min = 0.5 * (col_min + col_min5);
        col_max = 0.5 * (col_max + col_max5);
        col_avg = 0.5 * (col_avg + col_avg5);
    #endif

    #if defined(USE_YCoCg)
        vec2 chroma_extent = vec2(0.25 * 0.5 * (col_max.r - col_min.r));
        vec2 chroma_center = col_curr.gb;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif

    vec3 closest_frag = find_closest_fragment_3x3(g_depth, norm_uvs, texel_size);
    vec2 closest_vel = textureLod(g_velocity, closest_frag.xy, 0.0).rg;
#endif

    vec3 col_hist = SampleColor(g_color_hist, norm_uvs - closest_vel);

#if defined(USE_CLIPPING)
    col_hist = clip_aabb(col_min, col_max, col_hist);
#else
    col_hist = clamp(col_hist, col_min, col_max);
#endif

    const float HistoryWeightMin = 0.88;
    const float HistoryWeightMax = 0.97;

    float lum_curr = Luma(col_curr);
    float lum_hist = Luma(col_hist);

    float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.2);
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

    vec3 col = mix(col_curr, col_hist, history_weight);
#if defined(USE_YCoCg)
    col = YCoCg_to_RGB(col);
#endif
    g_out_color = TonemapInvert(col);
    g_out_history = g_out_color;
#endif // USE_STATIC_ACCUMULATION
}
