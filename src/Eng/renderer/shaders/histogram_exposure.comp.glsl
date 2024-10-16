#version 430 core

#include "_cs_common.glsl"
#include "histogram_exposure_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = HISTOGRAM_TEX_SLOT) uniform usampler2D g_histogram;
layout(binding = EXPOSURE_PREV_TEX_SLOT) uniform sampler2D g_exposure_prev;

layout(binding = OUT_TEX_SLOT, r32f) uniform image2D g_out_img;

float luma_from_histogram(const float hist) {
	return exp2((hist - 0.556) / 0.03);
}

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    const float sum = float(texelFetch(g_histogram, ivec2(EXPOSURE_HISTOGRAM_RES, 0), 0).x);

    vec2 total = vec2(0.0), bounds = vec2(0.4, 0.98) * sum;
    for (int i = 0; i < EXPOSURE_HISTOGRAM_RES; ++i) {
        float val = float(texelFetch(g_histogram, ivec2(i, 0), 0).x);
        float sub = min(val, bounds.x);
        val -= sub;
        bounds -= vec2(sub);
        val = min(val, bounds.y);
        bounds.y -= val;
        total += vec2(luma_from_histogram(float(i) / float(EXPOSURE_HISTOGRAM_RES)), 1.0) * val;
    }

    const float avg_luma = total.x / max(0.00001, total.y);
    const float exposure_curr = (g_params.exposure_factor / avg_luma);
    const float exposure_prev = texelFetch(g_exposure_prev, ivec2(0), 0).x;

    const float k = (exposure_curr < exposure_prev) ? 0.15 : 0.01;
    const float exposure = clamp(sqrt((1.0 - k) * sqr(exposure_prev) + k * sqr(exposure_curr)), g_params.min_exposure, g_params.max_exposure);
    imageStore(g_out_img, ivec2(0), vec4(exposure));
}