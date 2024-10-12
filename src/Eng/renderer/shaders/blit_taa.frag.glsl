#version 430 core

#include "_fs_common.glsl"
#include "taa_common.glsl"
#include "blit_taa_interface.h"

#pragma multi_compile _ CATMULL_ROM
#pragma multi_compile _ ROUNDED_NEIBOURHOOD
#pragma multi_compile _ TONEMAP
#pragma multi_compile _ YCoCg
#pragma multi_compile _ MOTION_BLUR
#pragma multi_compile _ STATIC_ACCUMULATION

#if defined(STATIC_ACCUMULATION) && (defined(CATMULL_ROM) || defined(ROUNDED_NEIBOURHOOD) || defined(TONEMAP) || defined(YCoCg) || defined(MOTION_BLUR))
    #pragma dont_compile
#endif

layout(binding = CURR_NEAREST_TEX_SLOT) uniform sampler2D g_color_curr_nearest;
layout(binding = CURR_LINEAR_TEX_SLOT) uniform sampler2D g_color_curr_linear;
layout(binding = HIST_TEX_SLOT) uniform sampler2D g_color_hist;

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_curr;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity;

layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exposure;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    SharedData g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;
layout(location = 1) out vec4 g_out_history;

float PDnrand(vec2 n) {
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 MaybeTonemap(vec3 c) {
#if defined(TONEMAP)
    c *= HDR_FACTOR * texelFetch(g_exposure, ivec2(0), 0).x;
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(vec3 c) {
#if defined(TONEMAP)
    c = c / (vec3(1.0) - c);
    c /= HDR_FACTOR * texelFetch(g_exposure, ivec2(0), 0).x;
#endif
    return c;
}

vec3 MaybeRGB_to_YCoCg(vec3 c) {
#if defined(YCoCg)
    c = RGB_to_YCoCg(c);
#endif
    return c;
}

vec4 FetchColor(sampler2D s, ivec2 icoord) {
    vec4 ret = texelFetch(s, icoord, 0);
    ret.xyz = MaybeRGB_to_YCoCg(MaybeTonemap(clamp(ret.xyz, vec3(0.0), vec3(HALF_MAX))));
    return ret;
}

vec4 SampleColor(sampler2D s, vec2 uvs) {
#if defined(CATMULL_ROM)
    vec4 ret = SampleTextureCatmullRom(s, uvs, g_params.tex_size);
#else
    vec4 ret = textureLod(s, uvs, 0.0);
#endif
    ret.xyz = MaybeTonemap(clamp(ret.xyz, vec3(0.0), vec3(HALF_MAX)));
    ret.xyz = MaybeRGB_to_YCoCg(ret.xyz);
    return ret;
}

vec4 SampleColorLinear(sampler2D s, const vec2 uvs) {
    vec4 ret = textureLod(s, uvs, 0.0);
    ret.xyz = MaybeRGB_to_YCoCg(MaybeTonemap(clamp(ret.xyz, vec3(0.0), vec3(HALF_MAX))));
    return ret;
}

vec4 SampleColorMotion(sampler2D s, vec2 uv, vec2 vel) {
    const vec2 v = 0.5 * vel;
    const int SampleCount = 3;

    float srand = 2.0 * PDnrand(uv + vec2(g_params.frame_index)) - 1.0;
    vec2 vtap = v / float(SampleCount);
    vec2 pos0 = uv + vtap * (0.5 * srand);
    vec4 accu = vec4(0.0);
    float wsum = 0.0;

    for (int i = -SampleCount; i <= SampleCount; ++i) {
        const vec2 motion_uv = pos0 + float(i) * vtap;
        const float w = saturate(motion_uv) == motion_uv ? 1.0 : 0.0;
        accu += w * SampleColorLinear(s, motion_uv);
        wsum += w;
    }

    return accu / wsum;
}

float Luma(vec3 col) {
#if defined(YCoCg)
    return col.r;
#else
    return dot(col, vec3(0.2125, 0.7154, 0.0721));
#endif
}

void main() {
    ivec2 uvs_px = ivec2(g_vtx_uvs);
    vec2 texel_size = vec2(1.0) / g_params.tex_size;
    vec2 norm_uvs = g_vtx_uvs / g_params.tex_size;

    const float depth = texelFetch(g_depth_curr, uvs_px, 0).r;
    vec4 col_curr = SampleColorLinear(g_color_curr_nearest, norm_uvs);

#if defined(STATIC_ACCUMULATION)
    vec4 col_hist = FetchColor(g_color_hist, uvs_px);
    vec3 col = mix(col_hist.xyz, col_curr.xyz, g_params.mix_factor);

    g_out_color = vec4(TonemapInvert(col), 0.0);
    g_out_history = g_out_color;
#else // STATIC_ACCUMULATION
    float min_depth = texelFetch(g_depth_curr, uvs_px, 0).r;

    const ivec2 offsets[8] = ivec2[8](
        ivec2(-1, 1),   ivec2(0, 1),    ivec2(1, 1),
        ivec2(-1, 0),                   ivec2(1, 0),
        ivec2(-1, -1),  ivec2(1, -1),   ivec2(1, -1)
    );

#if !defined(YCoCg) && !defined(ROUNDED_NEIBOURHOOD)
    vec3 col_avg = col_curr.xyz, col_var = col_curr.xyz * col_curr.xyz;
    ivec2 closest_frag = ivec2(0, 0);

    for (int i = 0; i < 8; i++) {
        const float depth = texelFetch(g_depth_curr, uvs_px + offsets[i], 0).r;
        if (depth > min_depth) {
            closest_frag = offsets[i];
            min_depth = depth;
        }

        vec3 col = FetchColor(g_color_curr_nearest, uvs_px + offsets[i]).xyz;
        col_avg += col;
        col_var += col * col;
    }

    col_avg /= 9.0;
    col_var /= 9.0;

    vec3 sigma = sqrt(max(col_var - col_avg * col_avg, vec3(0.0)));
    vec3 col_min = col_avg - 1.25 * sigma;
    vec3 col_max = col_avg + 1.25 * sigma;

    vec3 closest_vel = texelFetch(g_velocity, clamp(uvs_px + closest_frag, ivec2(0), ivec2(g_params.tex_size - vec2(1))), 0).xyz;
#else
    vec3 col_tl = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(-1, -1)).xyz));
    vec3 col_tc = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(+0, -1)).xyz));
    vec3 col_tr = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(+1, -1)).xyz));
    vec3 col_ml = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(-1, +0)).xyz));
    vec3 col_mc = col_curr.xyz;
    vec3 col_mr = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(+1, +0)).xyz));
    vec3 col_bl = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(-1, +1)).xyz));
    vec3 col_bc = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(+0, +1)).xyz));
    vec3 col_br = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, norm_uvs, 0.0, ivec2(+1, +1)).xyz));

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
        vec2 chroma_center = 0.5 * (col_min.yz + col_max.yz);//col_curr.gb;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif

    vec3 closest_frag = FindClosestFragment_3x3(g_depth_curr, norm_uvs, texel_size);
    vec3 closest_vel = textureLod(g_velocity, closest_frag.xy, 0.0).xyz;
#endif

    vec2 hist_uvs = norm_uvs - (closest_vel.xy / g_params.tex_size);
    vec4 col_hist = col_curr;
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        col_hist = SampleColor(g_color_hist, hist_uvs);
    }

    // Relax neighbourhood clamping/clipping if variance is high
    if (depth > 0.0 && g_params.significant_change < 0.5) {
        const float blend = saturate(1.0 - length(closest_vel.xy) / 4.0);
        col_min.x = col_min.x - blend * sqrt(col_hist.w);
        col_max.x = col_max.x + blend * sqrt(col_hist.w);
    #if defined(YCoCg)
        vec2 chroma_extent = vec2(0.25 * 0.5 * (col_max.r - col_min.r));
        vec2 chroma_center = 0.5 * (col_min.yz + col_max.yz);//col_curr.gb;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif
    }

    col_hist.xyz = ClipAABB(col_min, col_max, col_hist.xyz);
    //col_hist.xyz = clamp(col_hist.xyz, col_min, col_max);

    const float HistoryWeightMin = 0.88;
    const float HistoryWeightMax = 0.97;

    float lum_curr = Luma(col_curr.xyz);
    float lum_hist = Luma(col_hist.xyz);

    float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.2);
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

    vec3 col_temporal = mix(col_curr.xyz, col_hist.xyz, history_weight);
#if defined(YCoCg)
    col_temporal = YCoCg_to_RGB(col_temporal);
#endif
    vec3 col_screen = col_temporal;

#if defined(MOTION_BLUR)
    const float MotionScale = 0.5;
    closest_vel *= MotionScale;

    const float vel_mag = length(closest_vel.xy);
    const float vel_trust_full = 2.0;
    const float vel_trust_none = 15.0;
    const float vel_trust_span = vel_trust_none - vel_trust_full;
    const float trust = 1.0 - clamp(vel_mag - vel_trust_full, 0.0, vel_trust_span) / vel_trust_span;

    vec3 col_motion = SampleColorMotion(g_color_curr_linear, norm_uvs, (closest_vel.xy / g_params.tex_size)).xyz;
#if defined(YCoCg)
    col_motion = YCoCg_to_RGB(col_motion);
#endif
    col_screen = mix(col_motion, col_temporal, trust);
#endif // MOTION_BLUR

    const float variance = mix(unbiased_diff * unbiased_diff, col_hist.w, history_weight);
    const float k = saturate(2.0 - length(closest_vel.xy));

    g_out_color = clamp(vec4(TonemapInvert(col_screen), variance * k), vec4(0.0), vec4(HALF_MAX));
    g_out_history = clamp(vec4(TonemapInvert(col_temporal), variance * k), vec4(0.0), vec4(HALF_MAX));
#endif // STATIC_ACCUMULATION
}
