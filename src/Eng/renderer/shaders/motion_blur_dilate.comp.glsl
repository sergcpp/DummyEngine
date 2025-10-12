#version 430 core

#include "_cs_common.glsl"
#include "motion_blur_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = TILES_TEX_SLOT) uniform sampler2D g_tiles_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord) + 0.5) / vec2(g_params.img_size);

    vec3 tile_data[9];
    tile_data[0] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(-1, -1)).xyz;
    tile_data[1] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+0, -1)).xyz;
    tile_data[2] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+1, -1)).xyz;
    tile_data[3] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(-1, +0)).xyz;
    tile_data[4] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+0, +0)).xyz;
    tile_data[5] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+1, +0)).xyz;
    tile_data[6] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(-1, +1)).xyz;
    tile_data[7] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+0, +1)).xyz;
    tile_data[8] = textureLodOffset(g_tiles_tex, norm_uvs, 0.0, ivec2(+1, +1)).xyz;

    vec3 ret = tile_data[0];
    float max_vel_sq = length2(ret.xy);
    for (int i = 1; i < 9; ++i) {
        const float vel_sq = length2(tile_data[i].xy);
        if (vel_sq > max_vel_sq) {
            ret.xy = tile_data[i].xy;
            max_vel_sq = vel_sq;
        }
        ret.z = min(ret.z, tile_data[i].z);
    }

    imageStore(g_out_img, icoord, vec4(ret, 0.0));
}
