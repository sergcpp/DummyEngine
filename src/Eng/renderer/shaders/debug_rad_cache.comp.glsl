#version 430 core
#extension GL_NV_gpu_shader5 : enable // remove this!!!!

#include "_cs_common.glsl"
#include "rad_cache_common.glsl"
#include "debug_rad_cache_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;

layout(binding = OUT_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 px_coords = gl_GlobalInvocationID.xy;
    if (px_coords.x >= g_params.img_size.x || px_coords.y >= g_params.img_size.y) {
        return;
    }
    const vec2 norm_uvs = (vec2(px_coords) + 0.5) / vec2(g_params.img_size);
    const uvec2 ucoord = uvec2(norm_uvs * g_shrd_data.fren_res.xy);

    const float depth = texelFetch(g_depth_tex, ivec2(ucoord), 0).x;
    if (depth == 0.0) {
        imageStore(g_out_img, ivec2(px_coords), vec4(0, 0, 0, 1));
        return;
    }
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec4 normal_ws = UnpackNormalAndRoughness(texelFetch(g_normal_tex, ivec2(ucoord), 0).x);

    cache_grid_params_t grid;
    grid.cam_pos_curr = g_shrd_data.cam_pos_and_exp.xyz;
    grid.cam_pos_prev = grid.cam_pos_curr;
    grid.log_base = RAD_CACHE_GRID_LOGARITHM_BASE;
    grid.scale = RAD_CACHE_GRID_SCALE;
    grid.exposure = 1.0;

    const vec3 debug_color = hash_grid_debug(pos_ws, normal_ws.xyz, grid);
    imageStore(g_out_img, ivec2(px_coords), vec4(debug_color, 1.0));
}