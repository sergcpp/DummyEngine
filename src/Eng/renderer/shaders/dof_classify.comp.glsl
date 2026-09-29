#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"
#include "pmj_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = RANDOM_SEQ_BUF_SLOT) uniform usamplerBuffer g_random_seq;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const uint seed = get_pixel_seed(icoord, g_params.seed);
    // Tile origin in normalized screen space (depth texture may differ in resolution from the color image)
    const vec2 tile_uv_min = vec2(int(TILE_RES) * icoord) / vec2(g_params.img_size.zw);

    const int NUM_SAMPLES = 20;

    float max_depth = -1e9;
    float max_coc = -1e9;
    float min_depth = 1e9;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        const vec2 r = get_scrambled_2d_rand(g_random_seq, RAND_DIM_DOF, seed, uint(i));
        const vec2 uv = tile_uv_min + vec2(TILE_RES) * r / vec2(g_params.img_size.zw);
        const float depth = LinearizeDepth(textureLod(g_depth_tex, uv, 0.0).x, g_params.clip_info);
        const float coc = min(MAX_KERNEL_RADIUS, BlurRadiusFromDelta(depth, FOCAL_DEPTH, COC_FACTOR, FOCAL_LENGTH));
        max_depth = max(max_depth, depth);
        max_coc = max(max_coc, coc);
        min_depth = min(min_depth, depth);
    }

    // Clamp the tile's far extreme: a tile entirely beyond 5x the focal depth would flip PRESORT's mid_tile
    // sign and stay fully in the foreground layer (sharp distant sky). Far extreme = max_depth here.
    max_depth = min(max_depth, FOCAL_DEPTH * 5.0);

    imageStore(g_out_img, icoord, vec4(max_depth, max_coc, min_depth, 1.0));
}
