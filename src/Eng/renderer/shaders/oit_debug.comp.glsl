#version 430 core

#include "_cs_common.glsl"
#include "oit_debug_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = OIT_DEPTH_BUF_SLOT) uniform usamplerBuffer g_oit_depth_buf;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    ivec2 px_coords = ivec2(gl_GlobalInvocationID.xy);
    if (px_coords.x >= g_params.img_size.x || px_coords.y >= g_params.img_size.y) {
        return;
    }

    int frag_index = g_params.layer_index * g_params.img_size.x * g_params.img_size.y;
    frag_index += px_coords.y * g_params.img_size.x + px_coords.x;

    const float depth = uintBitsToFloat(texelFetch(g_oit_depth_buf, frag_index).x);
    imageStore(g_out_img, px_coords, vec4(compress_hdr(vec3(depth, 0, 0), 1.0), 0));
}
