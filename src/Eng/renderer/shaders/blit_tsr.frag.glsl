#version 430 core
#extension GL_EXT_control_flow_attributes : require

#include "_fs_common.glsl"
#include "taa_common.glsl"
#include "blit_tsr_interface.h"

#pragma multi_compile _ CATMULL_ROM
#pragma multi_compile _ ROUNDED_NEIBOURHOOD
#pragma multi_compile _ TONEMAP
#pragma multi_compile _ YCoCg
#pragma multi_compile _ LOCKING
#pragma multi_compile _ STATIC_ACCUMULATION

#if defined(STATIC_ACCUMULATION) && (defined(CATMULL_ROM) || defined(ROUNDED_NEIBOURHOOD) || defined(TONEMAP) || defined(YCoCg) || defined(MOTION_BLUR) || defined(LOCKING))
    #pragma dont_compile
#endif
#if defined(ROUNDED_NEIBOURHOOD) && !defined(YCoCg)
    #pragma dont_compile
#endif
#if defined(LOCKING) && !defined(YCoCg)
    #pragma dont_compile
#endif

layout(binding = CURR_NEAREST_TEX_SLOT) uniform sampler2D g_color_curr_nearest;
layout(binding = CURR_LINEAR_TEX_SLOT) uniform sampler2D g_color_curr_linear;
layout(binding = HIST_TEX_SLOT) uniform sampler2D g_color_hist;

layout(binding = DILATED_DEPTH_TEX_SLOT) uniform sampler2D g_dilated_depth;
layout(binding = DILATED_VELOCITY_TEX_SLOT) uniform sampler2D g_dilated_velocity;
layout(binding = DISOCCLUSION_MASK_TEX_SLOT) uniform sampler2D g_disocclusion_mask;

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(location = 0) in vec2 g_vtx_uvs;

layout(location = 0) out vec4 g_out_color;
layout(location = 1) out vec4 g_out_history;

// https://gpuopen.com/optimized-reversible-tonemapper-for-resolve/
vec3 MaybeTonemap(vec3 c) {
    c = clamp(c, vec3(0.0), vec3(HALF_MAX));
#if defined(TONEMAP)
    c = c / (c + vec3(1.0));
#endif
    return c;
}

vec3 TonemapInvert(vec3 c) {
#if defined(TONEMAP)
    c = c / (vec3(1.0) - c);
#endif
    c = clamp(c, vec3(0.0), vec3(HALF_MAX));
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
    ret.xyz = MaybeRGB_to_YCoCg(MaybeTonemap(ret.xyz));
    return ret;
}

vec4 SampleColor(sampler2D s, vec2 uvs) {
#if defined(CATMULL_ROM)
    vec4 ret = SampleTextureCatmullRom(s, uvs, g_params.texel_size.zw);
#else
    vec4 ret = textureLod(s, uvs, 0.0);
#endif
    ret.xyz = MaybeTonemap(ret.xyz);
    ret.xyz = MaybeRGB_to_YCoCg(ret.xyz);
    return ret;
}

vec4 SampleColorLinear(sampler2D s, const vec2 uvs) {
    vec4 ret = textureLod(s, uvs, 0.0);
    ret.xyz = MaybeTonemap(ret.xyz);
    return ret;
}

float Luma(vec3 col) {
#if defined(YCoCg)
    return col.x;
#else
    return lum(col);
#endif
}

float ComputeMaxKernelWeight() {
    const float KernelSizeBias = 1.0;
    const float kernelWeight = 1.0 + (1.0 / g_params.downscale_factor - 1.0) * KernelSizeBias;
    return min(1.99, kernelWeight);
}

// FSR1 lanczos approximation (https://www.desmos.com/calculator/ch1eezc3tq)
float Lanczos2ApproxSqNoClamp(const float x2) {
    const float a = (2.0 / 5.0) * x2 - 1.0;
    const float b = (1.0 / 4.0) * x2 - 1.0;
    return ((25.0 / 16.0) * a * a - (25.0 / 16.0 - 1.0)) * (b * b);
}

float Lanczos2ApproxSq(float x2) {
    x2 = min(x2, 4.0);
    return Lanczos2ApproxSqNoClamp(x2);
}

float GetUpsampleLanczosWeight(const vec2 sample_off, const float kernel_weight) {
    const vec2 sample_off_biased = sample_off * kernel_weight;
    return Lanczos2ApproxSq(dot(sample_off_biased, sample_off_biased));
}

void main() {
    const ivec2 uvs_px = ivec2(g_vtx_uvs);
    const vec2 norm_uvs = g_vtx_uvs * g_params.texel_size.zw;

#if defined(STATIC_ACCUMULATION)
    vec4 col_curr = textureLod(g_color_curr_nearest, norm_uvs, 0.0);
    col_curr.xyz = MaybeRGB_to_YCoCg(MaybeTonemap(col_curr.xyz));
    if (col_curr.w >= 1.0) {
        col_curr.w -= 1.0;
    }

    const vec4 col_hist = FetchColor(g_color_hist, uvs_px);
    const vec3 col = mix(col_hist.xyz, col_curr.xyz, g_params.mix_factor);

    g_out_color = vec4(TonemapInvert(col), 0.0);
    g_out_history = g_out_color;
#else // STATIC_ACCUMULATION
    const vec2 output_pos = vec2(uvs_px) + 0.5;
    const vec2 input_pos = output_pos * g_params.downscale_factor;
    const ivec2 i_input_pos = ivec2(floor(input_pos));

    vec2 unjitter = g_params.unjitter;
#if defined(VULKAN)
    unjitter.y = -unjitter.y;
#endif
    const vec2 unjittered_input_pos = vec2(i_input_pos) + 0.5 + unjitter;

    const vec2 base_uv = (vec2(i_input_pos) + 0.5) * g_params.texel_size.xy;

    vec4 col_curr = textureLod(g_color_curr_nearest, base_uv, 0.0);
    col_curr.xyz = MaybeRGB_to_YCoCg(MaybeTonemap(col_curr.xyz));
    if (col_curr.w >= 1.0) {
        col_curr.w -= 1.0;
    }

    const vec2 disocclusion = textureLod(g_disocclusion_mask, base_uv, 0.0).xy;

    // Load 3x3 neighborhood
    const vec3 col_tl = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(-1, -1)).xyz));
    const vec3 col_tc = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(+0, -1)).xyz));
    const vec3 col_tr = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(+1, -1)).xyz));
    const vec3 col_ml = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(-1, +0)).xyz));
    const vec3 col_mc = col_curr.xyz;
    const vec3 col_mr = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(+1, +0)).xyz));
    const vec3 col_bl = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(-1, +1)).xyz));
    const vec3 col_bc = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(+0, +1)).xyz));
    const vec3 col_br = MaybeRGB_to_YCoCg(MaybeTonemap(textureLodOffset(g_color_curr_nearest, base_uv, 0.0, ivec2(+1, +1)).xyz));

    const float kernel_reactive_factor = 0.075;
    const float kernel_bias_max = ComputeMaxKernelWeight() * (1.0 - kernel_reactive_factor);

    const float kernel_bias_min = max(1.0, (1.0 + kernel_bias_max) * 0.3);
    const float kernel_bias_factor = max(0.0, max(0.25 * disocclusion.x, kernel_reactive_factor));
    const float kernel_bias = mix(kernel_bias_max, kernel_bias_min, kernel_bias_factor);

    const vec2 base_sample_off = unjittered_input_pos - input_pos;

    vec4 color_weight = vec4(0.0);

    float weight;
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(-1.0, -1.0), kernel_bias);
    color_weight += vec4(col_tl * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+0.0, -1.0), kernel_bias);
    color_weight += vec4(col_tc * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+1.0, -1.0), kernel_bias);
    color_weight += vec4(col_tr * weight, weight);

    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(-1.0, +0.0), kernel_bias);
    color_weight += vec4(col_ml * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+0.0, +0.0), kernel_bias);
    color_weight += vec4(col_mc * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+1.0, +0.0), kernel_bias);
    color_weight += vec4(col_mr * weight, weight);

    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(-1.0, +1.0), kernel_bias);
    color_weight += vec4(col_bl * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+0.0, +1.0), kernel_bias);
    color_weight += vec4(col_bc * weight, weight);
    weight = GetUpsampleLanczosWeight(base_sample_off + vec2(+1.0, +1.0), kernel_bias);
    color_weight += vec4(col_br * weight, weight);

    vec3 col_min = min3(min3(col_tl, col_tc, col_tr),
                        min3(col_ml, col_mc, col_mr),
                        min3(col_bl, col_bc, col_br));
    vec3 col_max = max3(max3(col_tl, col_tc, col_tr),
                        max3(col_ml, col_mc, col_mr),
                        max3(col_bl, col_bc, col_br));

    vec3 col_avg = (col_tl + col_tc + col_tr + col_ml + col_mc + col_mr + col_bl + col_bc + col_br) / 9.0;

    #if defined(ROUNDED_NEIBOURHOOD)
        const vec3 col_min5 = min(col_tc, min(col_ml, min(col_mc, min(col_mr, col_bc))));
        const vec3 col_max5 = max(col_tc, max(col_ml, max(col_mc, max(col_mr, col_bc))));
        const vec3 col_avg5 = (col_tc + col_ml + col_mc + col_mr + col_bc) / 5.0;
        col_min = 0.5 * (col_min + col_min5);
        col_max = 0.5 * (col_max + col_max5);
        col_avg = 0.5 * (col_avg + col_avg5);
    #endif

    #if defined(YCoCg) && 0
        const vec2 chroma_extent = vec2(0.25 * 0.5 * (col_max.x - col_min.x));
        const vec2 chroma_center = 0.5 * (col_min.yz + col_max.yz);//col_curr.yz;
        col_min.yz = chroma_center - chroma_extent;
        col_max.yz = chroma_center + chroma_extent;
        col_avg.yz = chroma_center;
    #endif

    col_curr.xyz = (color_weight.xyz / color_weight.w);

    // Deringing
    col_curr.xyz = clamp(col_curr.xyz, col_min, col_max);

    bool set_lock = false;
#if defined(LOCKING)
    if (g_params.significant_change < 0.5) {
        const float diff_tl = max(col_tl.x, col_mc.x) / min(col_tl.x, col_mc.x);
        const float diff_tc = max(col_tc.x, col_mc.x) / min(col_tc.x, col_mc.x);
        const float diff_tr = max(col_tr.x, col_mc.x) / min(col_tr.x, col_mc.x);
        const float diff_ml = max(col_ml.x, col_mc.x) / min(col_ml.x, col_mc.x);
        const float diff_mr = max(col_mr.x, col_mc.x) / min(col_mr.x, col_mc.x);
        const float diff_bl = max(col_bl.x, col_mc.x) / min(col_bl.x, col_mc.x);
        const float diff_bc = max(col_bc.x, col_mc.x) / min(col_bc.x, col_mc.x);
        const float diff_br = max(col_br.x, col_mc.x) / min(col_br.x, col_mc.x);

        const float DissimilarThreshold = 1.75;

        float dissimilar_min = HALF_MAX, dissimilar_max = 0.0;
        uint mask = (1 << 4); // mark center as similar
        if (diff_tl > 0.0 && diff_tl < DissimilarThreshold) { mask |= (1 << 0); } else { dissimilar_min = min(dissimilar_min, col_tl.x); dissimilar_max = max(dissimilar_max, col_tl.x); }
        if (diff_tc > 0.0 && diff_tc < DissimilarThreshold) { mask |= (1 << 1); } else { dissimilar_min = min(dissimilar_min, col_tc.x); dissimilar_max = max(dissimilar_max, col_tc.x); }
        if (diff_tr > 0.0 && diff_tr < DissimilarThreshold) { mask |= (1 << 2); } else { dissimilar_min = min(dissimilar_min, col_tr.x); dissimilar_max = max(dissimilar_max, col_tr.x); }
        if (diff_ml > 0.0 && diff_ml < DissimilarThreshold) { mask |= (1 << 3); } else { dissimilar_min = min(dissimilar_min, col_ml.x); dissimilar_max = max(dissimilar_max, col_ml.x); }
        if (diff_mr > 0.0 && diff_mr < DissimilarThreshold) { mask |= (1 << 5); } else { dissimilar_min = min(dissimilar_min, col_mr.x); dissimilar_max = max(dissimilar_max, col_mr.x); }
        if (diff_bl > 0.0 && diff_bl < DissimilarThreshold) { mask |= (1 << 6); } else { dissimilar_min = min(dissimilar_min, col_bl.x); dissimilar_max = max(dissimilar_max, col_bl.x); }
        if (diff_bc > 0.0 && diff_bc < DissimilarThreshold) { mask |= (1 << 7); } else { dissimilar_min = min(dissimilar_min, col_bc.x); dissimilar_max = max(dissimilar_max, col_bc.x); }
        if (diff_br > 0.0 && diff_br < DissimilarThreshold) { mask |= (1 << 8); } else { dissimilar_min = min(dissimilar_min, col_br.x); dissimilar_max = max(dissimilar_max, col_br.x); }

        set_lock = col_mc.x > dissimilar_max || col_mc.x < dissimilar_min;

        //
        // Reject locking if any full quad is similar (it's not a tiny feature then)
        //
        // Upper left quad: + + -
        //                  + + -
        //                  - - -
        //
        set_lock = set_lock && ((mask & 0x01B) != 0x01B);
        //
        // Upper right quad: - + +
        //                   - + +
        //                   - - -
        //
        set_lock = set_lock && ((mask & 0x036) != 0x036);
        //
        // Bottom left quad: - - -
        //                   + + -
        //                   + + -
        //
        set_lock = set_lock && ((mask & 0x0D8) != 0x0D8);
        //
        // Bottom right quad: - - -
        //                    - + +
        //                    - + +
        //
        set_lock = set_lock && ((mask & 0x1B0) != 0x1B0);
    }
#endif // LOCKING

    vec2 closest_vel = textureLod(g_dilated_velocity, norm_uvs, 0.0).xy;
    const vec2 hist_uvs = norm_uvs - (closest_vel * g_params.texel_size.xy);
    vec4 col_hist = vec4(col_curr.xyz, 0.0);
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        col_hist = SampleColor(g_color_hist, hist_uvs);
    }

    const float lum_curr = Luma(col_curr.xyz);
    const float lum_hist = Luma(col_hist.xyz);

    float lock_status = col_hist.w;
#if defined(LOCKING)
    if (set_lock) {
        lock_status = (lock_status != 0.0 ? 2.0 : 1.0);
    } else {
        // decay lock with time
        lock_status = saturate(lock_status - (1.0 / 12.0));
    }

    if (any(lessThan(hist_uvs, 0.5 * g_params.texel_size.zw)) || any(greaterThan(hist_uvs, vec2(1.0) - 0.5 * g_params.texel_size.zw))) {
        lock_status = 0.0;
    }

    const float lum_diff = 1.0 - max(lum_curr, lum_hist) / min(lum_curr, lum_hist);
    if (lum_diff > 0.1) {
        lock_status = 0.0;
    }

    const float reactive_factor = saturate((lum_diff - 0.1) * 1.0);
    lock_status *= (1.0 - reactive_factor);

    // Specular luminance
    lock_status *= saturate(1.0 - 2.0 * (col_curr.w / col_curr.x));

    // Depth disocclusion
    lock_status *= float(disocclusion.x < 0.1);
    // Motion divergence
    lock_status *= (1.0 - disocclusion.y);
#endif // LOCKING

    const vec3 clamped_hist = ClipAABB(col_min, col_max, col_hist.xyz);

    const float history_contribution = saturate(4.0 * lock_status);
    col_hist.xyz = mix(clamped_hist, col_hist.xyz, history_contribution);

    const float HistoryWeightMin = 0.88;
    const float HistoryWeightMax = 0.93;

    const float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.2);
    const float unbiased_weight = 1.0 - unbiased_diff;
    const float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    const float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

    vec3 col_temporal = mix(col_curr.xyz, col_hist.xyz, history_weight);
#if defined(YCoCg)
    col_temporal = YCoCg_to_RGB(col_temporal);
#endif
    const vec3 col_screen = col_temporal;

    g_out_color = vec4(TonemapInvert(col_screen), lock_status);
    g_out_history = vec4(TonemapInvert(col_temporal), lock_status);
#endif // STATIC_ACCUMULATION
}
