#version 430 core

#include "_cs_common.glsl"
#include "tile_clear_interface.h"

#pragma multi_compile _ AVERAGE
#pragma multi_compile _ VARIANCE

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(std430, binding = TILE_LIST_BUF_SLOT) readonly buffer TileList {
    uint g_tile_list[];
};

layout(binding = OUT_RAD_IMG_SLOT, rgba16f) uniform writeonly image2D g_out_rad_img;
#ifdef AVERAGE
    layout(binding = OUT_AVG_RAD_IMG_SLOT, rgba16f) uniform writeonly image2D g_out_avg_rad_img;
#endif
#ifdef VARIANCE
    layout(binding = OUT_VARIANCE_IMG_SLOT, r16f) uniform writeonly image2D g_out_variance_img;
#endif

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uint packed_coords = g_tile_list[g_params.tile_count - gl_WorkGroupID.x - 1];
    const ivec2 dispatch_thread_id = ivec2(packed_coords & 0xffffu, (packed_coords >> 16) & 0xffffu) + ivec2(gl_LocalInvocationID.xy);

    imageStore(g_out_rad_img, dispatch_thread_id, vec4(0.0, 0.0, 0.0, -1.0));

#ifdef AVERAGE
    if (gl_LocalInvocationID.x == 0u && gl_LocalInvocationID.y == 0u) {
        imageStore(g_out_avg_rad_img, dispatch_thread_id / 8, vec4(0.0));
    }
#endif
#ifdef VARIANCE
    imageStore(g_out_variance_img, dispatch_thread_id, vec4(0.0));
#endif
}
