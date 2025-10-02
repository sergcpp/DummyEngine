#version 430 core
#extension GL_EXT_control_flow_attributes : enable

#include "_cs_common.glsl"
#include "gtao_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = GTAO_TEX_SLOT) uniform sampler2D g_gtao_tex;

layout(binding = OUT_IMG_SLOT, r8) uniform image2D g_out_img;

void Upsample(const ivec2 dispatch_thread_id, const ivec2 group_thread_id, const uvec2 screen_size) {
    const vec2 texel_size = vec2(1.0) / vec2(screen_size);
    const vec2 uvs = (vec2(dispatch_thread_id) + 0.5) * texel_size;
    const vec2 _uvs = (vec2(2 * (dispatch_thread_id / 2)) + 0.5) * texel_size;

    const float d0 = texelFetch(g_depth_tex, dispatch_thread_id, 0).x;

    const float d1 = abs(d0 - textureLodOffset(g_depth_tex, _uvs, 0.0, ivec2(+0, +0)).x);
    const float d2 = abs(d0 - textureLodOffset(g_depth_tex, _uvs, 0.0, ivec2(+0, +2)).x);
    const float d3 = abs(d0 - textureLodOffset(g_depth_tex, _uvs, 0.0, ivec2(+2, +0)).x);
    const float d4 = abs(d0 - textureLodOffset(g_depth_tex, _uvs, 0.0, ivec2(+2, +2)).x);

    const float dmin = min(min(d1, d2), min(d3, d4));

    float val = 0.0;
    if (dmin < 0.000001 && false) {
        val = textureLod(g_gtao_tex, uvs, 0.0).x;
    } else {
        if (dmin == d1) {
            val = textureLodOffset(g_gtao_tex, _uvs, 0.0, ivec2(+0, +0)).x;
        } else if (dmin == d2) {
            val = textureLodOffset(g_gtao_tex, _uvs, 0.0, ivec2(+0, +1)).x;
        } else if (dmin == d3) {
            val = textureLodOffset(g_gtao_tex, _uvs, 0.0, ivec2(+1, +0)).x;
        } else if (dmin == d4) {
            val = textureLodOffset(g_gtao_tex, _uvs, 0.0, ivec2(+1, +1)).x;
        }
    }

    imageStore(g_out_img, dispatch_thread_id, vec4(val));
}

layout(local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 group_id = gl_WorkGroupID.xy;
    const uint group_index = gl_LocalInvocationIndex;
    const uvec2 group_thread_id = RemapLane8x8(group_index);
    const uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    Upsample(ivec2(dispatch_thread_id), ivec2(group_thread_id), g_params.img_size.xy);
}