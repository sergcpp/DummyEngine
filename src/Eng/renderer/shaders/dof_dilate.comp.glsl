#version 430 core

#include "_cs_common.glsl"
#include "dof_interface.h"
#include "dof_common.glsl"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = COC_TILES_TEX_SLOT) uniform sampler2D g_coc_tiles_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    vec3 dilated_val = vec3(-1e9, -1e9, 1e9);

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const ivec2 c = clamp(icoord + ivec2(x - 1, y - 1), ivec2(0), ivec2(g_params.img_size.xy) - 1);
            const vec3 fetch = texelFetch(g_coc_tiles_tex, c, 0).xyz;
            dilated_val = vec3(max(dilated_val.xy, fetch.xy), min(dilated_val.z, fetch.z));
        }
    }

    imageStore(g_out_img, icoord, vec4(dilated_val, 1.0));
}
