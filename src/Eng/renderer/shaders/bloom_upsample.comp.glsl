#version 430 core

#include "_cs_common.glsl"
#include "bloom_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = INPUT_TEX_SLOT) uniform sampler2D g_input_tex;
layout(binding = BLEND_TEX_SLOT) uniform sampler2D g_blend_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform image2D g_out_img;

layout(local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

// Taken from "Next Generation Post Processing in Call of Duty Advanced Warfare"
void main() {
    if (gl_GlobalInvocationID.x >= g_params.img_size.x || gl_GlobalInvocationID.y >= g_params.img_size.y) {
        return;
    }

    const vec2 pix_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(g_params.img_size);

    vec3 result = vec3(0.0);
    result += 4.0 * textureLod(g_input_tex, pix_uv, 0.0).xyz;
    result += 2.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, +0)).xyz;
    result += 2.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, +0)).xyz;
    result += 2.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, +1)).xyz;
    result += 2.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+0, -1)).xyz;
    result += 1.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, -1)).xyz;
    result += 1.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, -1)).xyz;
    result += 1.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(-1, +1)).xyz;
    result += 1.0 * textureLodOffset(g_input_tex, pix_uv, 0.0, ivec2(+1, +1)).xyz;
    result /= 16.0;

    result = mix(result, texelFetch(g_blend_tex, ivec2(gl_GlobalInvocationID.xy), 0).xyz, g_params.blend_weight);

    imageStore(g_out_img, ivec2(gl_GlobalInvocationID.xy), vec4(result, 0.0));
}