#version 320 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "rt_shadow_debug_interface.h"
#include "rt_shadow_common.glsl.inl"

/*
PERM @NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && (!defined(GL_KHR_shader_subgroup_basic) || !defined(GL_KHR_shader_subgroup_ballot) || !defined(GL_KHR_shader_subgroup_shuffle) || !defined(GL_KHR_shader_subgroup_vote))
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = HIT_MASK_TEX_SLOT) uniform highp usampler2D g_hit_mask_tex;

layout(binding = OUT_RESULT_IMG_SLOT, r8) uniform writeonly image2D g_out_result_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uvec2 group_thread_id = gl_LocalInvocationID.xy;
    uvec2 dispatch_thread_id = gl_GlobalInvocationID.xy;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    uint sun_mask = texelFetch(g_hit_mask_tex, ivec2(group_id), 0).r;
    float visibility = (sun_mask & (1u << (group_thread_id.y * 8 + group_thread_id.x))) != 0 ? 0.0 : 1.0;

    imageStore(g_out_result_img, ivec2(dispatch_thread_id), vec4(visibility));
}
