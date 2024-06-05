#version 3420 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "gi_common.glsl"
#include "taa_common.glsl"
#include "gi_stabilization_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = GI_TEX_SLOT) uniform sampler2D g_gi_curr_tex;
layout(binding = GI_HIST_TEX_SLOT) uniform sampler2D g_gi_hist_tex;
layout(binding = EXPOSURE_TEX_SLOT) uniform sampler2D g_exp_tex;

layout(binding = OUT_GI_IMG_SLOT, rgba16f) uniform image2D g_out_gi_img;

float Luma(vec3 col) {
    return HDR_FACTOR * dot(col, vec3(0.2125, 0.7154, 0.0721));
}

void Stabilize(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size, float exposure) {
    const vec2 texel_size = vec2(1.0) / vec2(screen_size);
    const vec2 uvs = (vec2(dispatch_thread_id) + 0.5) * texel_size;

    const vec2 closest_frag = FindClosestFragment_3x3(g_depth_tex, uvs, texel_size).xy;
    const vec2 closest_vel = textureLod(g_velocity_tex, closest_frag.xy, 0.0).rg;

    const vec4 rad_curr = texelFetch(g_gi_curr_tex, dispatch_thread_id, 0) * vec4(exposure, exposure, exposure, 1.0);

    vec2 hist_uvs = uvs - closest_vel;
    vec4 rad_hist = rad_curr;
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        rad_hist = textureLod(g_gi_hist_tex, hist_uvs, 0.0) * vec4(exposure, exposure, exposure, 1.0);
    }

    { // neighbourhood clamp
        const vec3 rad_tl = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2(-1, -1)).rgb * exposure;
        const vec3 rad_tc = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2( 0, -1)).rgb * exposure;
        const vec3 rad_tr = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2( 1, -1)).rgb * exposure;
        const vec3 rad_ml = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2(-1,  0)).rgb * exposure;
        const vec3 rad_mc = rad_curr.rgb;
        const vec3 rad_mr = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2( 1,  0)).rgb * exposure;
        const vec3 rad_bl = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2(-1,  1)).rgb * exposure;
        const vec3 rad_bc = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2( 0,  1)).rgb * exposure;
        const vec3 rad_br = textureLodOffset(g_gi_curr_tex, uvs, 0.0, ivec2( 1,  1)).rgb * exposure;

        const vec3 rad_min = min3(min3(rad_tl, rad_tc, rad_tr),
                                  min3(rad_ml, rad_mc, rad_mr),
                                  min3(rad_bl, rad_bc, rad_br));
        const vec3 rad_max = max3(max3(rad_tl, rad_tc, rad_tr),
                                  max3(rad_ml, rad_mc, rad_mr),
                                  max3(rad_bl, rad_bc, rad_br));

        rad_hist.rgb = ClipAABB(rad_min, rad_max, rad_hist.rgb);
    }

    const float HistoryWeightMin = 0.95;
    const float HistoryWeightMax = 0.98;

    float lum_curr = Luma(rad_curr.rgb);
    float lum_hist = Luma(rad_hist.rgb);

    float unbiased_diff = abs(lum_curr - lum_hist) / max3(lum_curr, lum_hist, 0.2);
    float unbiased_weight = 1.0 - unbiased_diff;
    float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
    float history_weight = mix(HistoryWeightMin, HistoryWeightMax, unbiased_weight_sqr);

    const vec4 rad = mix(rad_curr, rad_hist, history_weight);
    imageStore(g_out_gi_img, dispatch_thread_id, vec4(rad.rgb / exposure, rad.w));
}

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    const float exposure = texelFetch(g_exp_tex, ivec2(0), 0).x;

    Stabilize(ivec2(dispatch_thread_id), ivec2(group_thread_id), g_params.img_size.xy, exposure);
}
