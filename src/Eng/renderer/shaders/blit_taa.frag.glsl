#version 320 es

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_fs_common.glsl"
#include "taa_common.glsl"
#include "blit_taa_interface.h"

#pragma multi_compile _ CLIPPING
#pragma multi_compile _ ROUNDED_NEIBOURHOOD
#pragma multi_compile _ TONEMAP
#pragma multi_compile _ YCoCg
#pragma multi_compile _ STATIC_ACCUMULATION

layout(binding = CURR_TEX_SLOT) uniform sampler2D g_color_curr;
layout(binding = HIST_TEX_SLOT) uniform sampler2D g_color_hist;

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity;

layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exposure;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in highp vec2 g_vtx_uvs;

layout(location = 0) out vec3 g_out_color;
layout(location = 1) out vec3 g_out_history;

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 Tonemap(in vec3 c) {
#if defined(TONEMAP)
    c *= texelFetch(g_exposure, ivec2(0), 0).x;
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(in vec3 c) {
#if defined(TONEMAP)
    c = c / (vec3(1.0) - c);
    c /= texelFetch(g_exposure, ivec2(0), 0).x;
#endif
    return c;
}

vec3 FetchColor(sampler2D s, ivec2 icoord) {
    vec3 ret = Tonemap(clamp(texelFetch(s, icoord, 0).rgb, vec3(0.0), vec3(HALF_MAX)));
#if defined(YCoCg)
    ret = RGB_to_YCoCg(ret);
#endif
    return ret;
}

vec3 SampleColor(sampler2D s, vec2 uvs) {
    vec3 ret = Tonemap(clamp(textureLod(s, uvs, 0.0).rgb, vec3(0.0), vec3(HALF_MAX)));
#if defined(YCoCg)
    ret = RGB_to_YCoCg(ret);
#endif
    return ret;
}

float Luma(vec3 col) {
#if defined(YCoCg)
    return HDR_FACTOR * col.r;
#else
    return HDR_FACTOR * dot(col, vec3(0.2125, 0.7154, 0.0721));
#endif
}

void main() {
    ivec2 uvs_px = ivec2(g_vtx_uvs);
    vec2 texel_size = vec2(1.0) / g_params.tex_size;
    vec2 norm_uvs = g_vtx_uvs / g_params.tex_size;

    vec3 col_curr = FetchColor(g_color_curr, uvs_px);

#if defined(STATIC_ACCUMULATION)
    vec3 col_hist = FetchColor(g_color_hist, uvs_px);
    vec3 col = mix(col_hist, col_curr, g_params.mix_factor);

    g_out_color = TonemapInvert(col);
    g_out_history = g_out_color;
#else // STATIC_ACCUMULATION
    float min_depth = texelFetch(g_depth, uvs_px, 0).r;

    const ivec2 offsets[8] = ivec2[8](
        ivec2(-1, 1),   ivec2(0, 1),    ivec2(1, 1),
        ivec2(-1, 0),                   ivec2(1, 0),
        ivec2(-1, -1),  ivec2(1, -1),   ivec2(1, -1)
    );

#if !defined(YCoCg) && !defined(ROUNDED_NEIBOURHOOD)
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

    #if defined(ROUNDED_NEIBOURHOOD)
        vec3 col_min5 = min(col_tc, min(col_ml, min(col_mc, min(col_mr, col_bc))));
        vec3 col_max5 = max(col_tc, max(col_ml, max(col_mc, max(col_mr, col_bc))));
        vec3 col_avg5 = (col_tc + col_ml + col_mc + col_mr + col_bc) / 5.0;
        col_min = 0.5 * (col_min + col_min5);
        col_max = 0.5 * (col_max + col_max5);
        col_avg = 0.5 * (col_avg + col_avg5);
    #endif

    #if defined(YCoCg)
        vec2 chroma_extent = vec2(0.25 * 0.5 * (col_max.r - col_min.r));
        vec2 chroma_center = col_curr.gb;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif

    vec3 closest_frag = FindClosestFragment_3x3(g_depth, norm_uvs, texel_size);
    vec2 closest_vel = textureLod(g_velocity, closest_frag.xy, 0.0).rg;
#endif

    vec2 hist_uvs = norm_uvs - closest_vel;
    vec3 col_hist = col_curr;
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        col_hist = SampleColor(g_color_hist, hist_uvs);
    }

#if defined(CLIPPING)
    col_hist = ClipAABB(col_min, col_max, col_hist);
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
#if defined(YCoCg)
    col = YCoCg_to_RGB(col);
#endif
    g_out_color = clamp(TonemapInvert(col), vec3(0.0), vec3(HALF_MAX));
    g_out_history = g_out_color;
#endif // STATIC_ACCUMULATION
}
