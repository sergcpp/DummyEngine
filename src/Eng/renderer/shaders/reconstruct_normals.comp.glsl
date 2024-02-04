#version 310 es
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "reconstruct_normals_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;

layout(binding = OUT_NORMALS_IMG_SLOT, rg16_snorm) uniform image2D g_out_normals_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    uvec2 group_thread_id = RemapLane8x8(group_index);
    uvec2 dispatch_thread_id = group_id * 8u + group_thread_id;
    if (dispatch_thread_id.x >= g_params.img_size.x || dispatch_thread_id.y >= g_params.img_size.y) {
        return;
    }

    vec2 uv0 = (vec2(dispatch_thread_id) + 0.5) / vec2(g_params.img_size);
    vec2 uv1 = uv0 + vec2(1.0, 0.0) / vec2(g_params.img_size);
    vec2 uv2 = uv0 + vec2(0.0, 1.0) / vec2(g_params.img_size);

    float depth0_vs = LinearizeDepth(textureLod(g_depth_tex, uv0, 0.0).r, g_params.clip_info);
    float depth1_vs = LinearizeDepth(textureLod(g_depth_tex, uv1, 0.0).r, g_params.clip_info);
    float depth2_vs = LinearizeDepth(textureLod(g_depth_tex, uv2, 0.0).r, g_params.clip_info);

    vec3 p0_vs = ReconstructViewPosition(uv0, g_params.frustum_info, -depth0_vs, 0.0 /* is_ortho */);
    vec3 p1_vs = ReconstructViewPosition(uv1, g_params.frustum_info, -depth1_vs, 0.0 /* is_ortho */);
    vec3 p2_vs = ReconstructViewPosition(uv2, g_params.frustum_info, -depth2_vs, 0.0 /* is_ortho */);

    vec3 normal = normalize(cross(p2_vs - p0_vs, p1_vs - p0_vs));
    imageStore(g_out_normals_img, ivec2(dispatch_thread_id), vec4(normal.xy, 0.0, 0.0));
}
