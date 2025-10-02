#version 430 core
#extension GL_ARB_shading_language_packing : require

#include "_cs_common.glsl"
#include "taa_common.glsl"
#include "gtao_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = DEPTH_HIST_TEX_SLOT) uniform sampler2D g_depth_hist_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;
layout(binding = GTAO_TEX_SLOT) uniform sampler2D g_gtao_tex;
layout(binding = GTAO_HIST_TEX_SLOT) uniform sampler2D g_gtao_hist_tex;

layout(binding = OUT_IMG_SLOT, r8) uniform image2D g_out_img;

float GetEdgeStoppingDepthWeight(float current_depth, float history_depth) {
    return exp(-abs(current_depth - history_depth) / current_depth * 32.0);
}

void Accumulate(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size) {
    const vec2 texel_size = vec2(1.0) / vec2(screen_size);
    const vec2 uvs = (vec2(dispatch_thread_id) + 0.5) * texel_size;

    const float depth_curr = LinearizeDepth(textureLod(g_depth_tex, uvs, 0.0).x, g_params.clip_info);
    const vec2 vel = textureLod(g_velocity_tex, uvs, 0.0).xy * texel_size;
    const float ao_curr = textureLod(g_gtao_tex, uvs, 0.0).x;

    vec2 hist_uvs = uvs - vel;
    float ao_hist = ao_curr, depth_hist = depth_curr;
    if (all(greaterThan(hist_uvs, vec2(0.0))) && all(lessThan(hist_uvs, vec2(1.0)))) {
        ao_hist = textureLod(g_gtao_hist_tex, hist_uvs, 0.0).x;
        depth_hist = LinearizeDepth(textureLod(g_depth_hist_tex, hist_uvs, 0.0).x, g_params.clip_info);
    }

    { // neighbourhood clamp
        const float ao_tl = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2(-1, -1)).x;
        const float ao_tc = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2( 0, -1)).x;
        const float ao_tr = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2( 1, -1)).x;
        const float ao_ml = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2(-1,  0)).x;
        const float ao_mc = ao_curr;
        const float ao_mr = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2( 1,  0)).x;
        const float ao_bl = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2(-1,  1)).x;
        const float ao_bc = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2( 0,  1)).x;
        const float ao_br = textureLodOffset(g_gtao_tex, uvs, 0.0, ivec2( 1,  1)).x;

        const float ao_min = min3(min3(ao_tl, ao_tc, ao_tr),
                                  min3(ao_ml, ao_mc, ao_mr),
                                  min3(ao_bl, ao_bc, ao_br));
        const float ao_max = max3(max3(ao_tl, ao_tc, ao_tr),
                                  max3(ao_ml, ao_mc, ao_mr),
                                  max3(ao_bl, ao_bc, ao_br));

        ao_hist = clamp(ao_hist, ao_min, ao_max);
    }

    const float ao = mix(ao_curr, ao_hist, 0.95 * GetEdgeStoppingDepthWeight(depth_curr, depth_hist));
    imageStore(g_out_img, dispatch_thread_id, vec4(ao));
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    Accumulate(ivec2(dispatch_thread_id), ivec2(group_thread_id), g_params.img_size.xy);
}
