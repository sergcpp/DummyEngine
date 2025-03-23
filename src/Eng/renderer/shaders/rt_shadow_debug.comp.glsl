#version 430 core

#include "_cs_common.glsl"
#include "rt_shadow_debug_interface.h"
#include "rt_shadow_common.glsl.inl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = HIT_MASK_TEX_SLOT) uniform usampler2D g_hit_mask_tex;

layout(binding = OUT_RESULT_IMG_SLOT, r8) uniform restrict writeonly image2D g_out_result_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uvec2 group_thread_id = gl_LocalInvocationID.xy;
    uvec2 dispatch_thread_id = gl_GlobalInvocationID.xy;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    uint sun_mask = texelFetch(g_hit_mask_tex, ivec2(group_id), 0).x;
    float visibility = (sun_mask & (1u << (group_thread_id.y * 8 + group_thread_id.x))) != 0 ? 0.0 : 1.0;

    imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(visibility));
}
