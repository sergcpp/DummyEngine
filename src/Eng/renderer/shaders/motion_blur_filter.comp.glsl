#version 430 core

#include "_cs_common.glsl"
#include "motion_blur_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = COLOR_TEX_SLOT) uniform sampler2D g_color_tex;
layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = TILES_TEX_SLOT) uniform sampler2D g_tiles_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

#define DEBUG_CLASSIFICATION 0

struct sample_data_t {
    vec4 color;
    float depth;
    float spread_px;
};

sample_data_t Sample(const vec2 uv) {
    sample_data_t ret;
    ret.color = textureLod(g_color_tex, uv, 0.0);
    ret.depth = LinearizeDepth(textureLod(g_depth_tex, uv, 0.0).x, g_params.clip_info);
    ret.spread_px = length(textureLod(g_velocity_tex, uv, 0.0).xy);
    return ret;
}

vec2 DepthCmp(float center_depth, float sample_depth) {
	return saturate(0.5 + vec2(0.1, -0.1) * (sample_depth - center_depth));
}

vec2 SpreadCmp(float offset_len, vec2 spread_len) {
	return saturate(spread_len - max(offset_len - 1.0, 0.0));
}

float GetSampleWeight(float center_depth, float sample_epth, float offset_len, float center_spread_len, float sample_spread_len) {
	return dot(DepthCmp(center_depth, sample_epth),
               SpreadCmp(offset_len, vec2(center_spread_len, sample_spread_len)));
}

const float DitherScale = 0.5;

float Dither2x2(const ivec2 icoord) {
    const vec2 position_mod = vec2(icoord & 1);
    return (-DitherScale + 2.0 * DitherScale * position_mod.x) * (-1.0 + 2.0 * position_mod.y);
}

float Dither4x4(const ivec2 icoord) {
    return 2.0 * DitherScale * Bayer4x4(uvec2(icoord), 0) - DitherScale;
}

float PDnrand(vec2 n) {
	return 2.0 * DitherScale * fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453) - DitherScale;
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const vec2 pix_uv = (vec2(icoord) + 0.5) / vec2(g_params.img_size);
    const vec3 tile_data = textureLod(g_tiles_tex, pix_uv, 0.0).xyz;

    sample_data_t center = Sample(pix_uv);

    if (false && length2(tile_data.xy) < 0.25) {
#if DEBUG_CLASSIFICATION
        center.color.rg *= 0.0;
#endif
        imageStore(g_out_img, icoord, center.color);
        return;
    }

    const int SampleCount = 4;
    const float DurationFrames = 0.5;

    //const float jitter = PDnrand(vec2(icoord));
    //const float jitter = Dither2x2(icoord);
    const float jitter = Dither4x4(icoord);

    const vec2 uv_delta = (1.0 + jitter) * (0.5 * DurationFrames / SampleCount) * tile_data.xy * g_params.inv_ren_res;
    const float px_delta_len = (0.5 * DurationFrames / SampleCount) * length(tile_data.xy);

    vec4 result = vec4(0.0);
    float total_weight = 0.0;

    if (abs(length2(tile_data.xy) - tile_data.z) < 12.0) {
        //
        // Simple color average (cheap)
        //
        vec4 uv_curr = vec4(pix_uv + 0.5 * uv_delta, pix_uv - 0.5 * uv_delta);
        for (int i = 0; i < SampleCount; ++i) {
            const bvec4 bounds_test = equal(saturate(uv_curr), uv_curr);
            vec2 w = vec2(all(bounds_test.xy), all(bounds_test.zw));

            result += w.x * Sample(uv_curr.xy).color;
            total_weight += w.x;

            result += w.y * Sample(uv_curr.zw).color;
            total_weight += w.y;

            uv_curr += vec4(uv_delta, -uv_delta);
        }
    } else {
        //
        // Foreground/background separation (expensive)
        //
        vec4 uv_curr = vec4(pix_uv + 0.5 * uv_delta, pix_uv - 0.5 * uv_delta);
        float sample_dist = 0.5 * px_delta_len;
        for (int i = 0; i < SampleCount; ++i) {
            const bvec4 bounds_test = equal(saturate(uv_curr), uv_curr);
            vec2 w = vec2(all(bounds_test.xy), all(bounds_test.zw));

            const sample_data_t s1 = Sample(uv_curr.xy);
            w.x *= GetSampleWeight(center.depth, s1.depth, sample_dist, center.spread_px, s1.spread_px);
            result += w.x * s1.color;
            total_weight += w.x;

            const sample_data_t s2 = Sample(uv_curr.zw);
            w.y *= GetSampleWeight(center.depth, s2.depth, sample_dist, center.spread_px, s2.spread_px);
            result += w.y * s2.color;
            total_weight += w.y;

            uv_curr += vec4(uv_delta, -uv_delta);
            sample_dist += px_delta_len;
        }
    }

    result *= 1.0 / (2 * SampleCount);
    total_weight *= 1.0 / (2 * SampleCount);

    // Add center sample contribution
    result = result + (1.0 - total_weight) * center.color;
    result = clamp(result, vec4(0.0), vec4(HALF_MAX));

#if DEBUG_CLASSIFICATION
    if (abs(length2(tile_data.xy) - tile_data.z) < 24.0) {
        result.rb *= 0.0;
    } else {
        result.gb *= 0.0;
    }
#endif

    imageStore(g_out_img, icoord, result);
}