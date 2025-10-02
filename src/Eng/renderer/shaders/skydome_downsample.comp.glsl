#version 430 core

#include "_cs_common.glsl"
#include "skydome_downsample_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = INPUT_TEX_SLOT) uniform sampler2D g_input_tex;

layout(binding = OUTPUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_output_img[4];

shared vec4 g_shared[8][8];

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const vec2 norm_uvs = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(g_params.img_size);

    const vec4 res = textureLod(g_input_tex, norm_uvs, 0);
    imageStore(g_output_img[0], ivec2(gl_GlobalInvocationID.xy), res);

    if (g_params.mip_count < 2) {
        return;
    }

    ivec2 work_group_id = ivec2(gl_WorkGroupID.xy);
    uint x = gl_LocalInvocationID.x, y = gl_LocalInvocationID.y;

    g_shared[y][x] = res;
    groupMemoryBarrier(); barrier();

    if (x < 4 && y < 4) {
        const vec4 v = 0.25 * (g_shared[2 * y + 0][2 * x + 0] +
                               g_shared[2 * y + 0][2 * x + 1] +
                               g_shared[2 * y + 1][2 * x + 0] +
                               g_shared[2 * y + 1][2 * x + 1]);
        imageStore(g_output_img[1], 4 * work_group_id + ivec2(x, y), v);
        g_shared[2 * y][2 * x] = v;
    }
    groupMemoryBarrier(); barrier();
    if (g_params.mip_count < 3) {
        return;
    }
    if (x < 2 && y < 2) {
        const vec4 v = 0.25 * (g_shared[4 * y + 0][4 * x + 0] +
                               g_shared[4 * y + 0][4 * x + 2] +
                               g_shared[4 * y + 2][4 * x + 0] +
                               g_shared[4 * y + 2][4 * x + 2]);
        imageStore(g_output_img[2], 2 * work_group_id + ivec2(x, y), v);
        g_shared[4 * y][4 * x] = v;
    }
    groupMemoryBarrier(); barrier();
    if (g_params.mip_count < 4) {
        return;
    }
    if (x < 1 && y < 1) {
        const vec4 v = 0.25 * (g_shared[8 * y + 0][8 * x + 0] +
                               g_shared[8 * y + 0][8 * x + 4] +
                               g_shared[8 * y + 4][8 * x + 0] +
                               g_shared[8 * y + 4][8 * x + 4]);
        imageStore(g_output_img[3], 1 * work_group_id + ivec2(x, y), v);
    }
}
