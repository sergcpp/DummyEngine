#version 430 core

#include "_cs_common.glsl"
#include "debug_image_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = INPUT_TEX_SLOT) uniform sampler2D g_input_tex;

layout(binding = OUT_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 px_coords = gl_GlobalInvocationID.xy;
    if (px_coords.x >= g_params.img_size.x || px_coords.y >= g_params.img_size.y) {
        return;
    }

    const vec4 in_color = texelFetch(g_input_tex, ivec2(px_coords), 0);

    const vec4 out_color = vec4(vec3(in_color[g_params.channel]), 1.0);
    imageStore(g_out_img, ivec2(px_coords), out_color);
}
