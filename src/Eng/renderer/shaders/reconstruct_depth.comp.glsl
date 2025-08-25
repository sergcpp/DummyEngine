#version 430 core

#include "_cs_common.glsl"
#include "taa_common.glsl"
#include "reconstruct_depth_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;

layout(binding = OUT_RECONSTRUCTED_DEPTH_IMG_SLOT, r32ui) uniform coherent uimage2D g_out_reconstructed_depth_img;
layout(binding = OUT_DILATED_DEPTH_IMG_SLOT, r32f) uniform image2D g_out_dilated_depth_img;
layout(binding = OUT_DILATED_VELOCITY_IMG_SLOT, rg16f) uniform image2D g_out_dilated_velocity_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

    const vec2 norm_uvs = (vec2(icoord) + 0.5) * g_params.texel_size;
    const vec3 closest_frag = FindClosestFragment_3x3(g_depth_tex, norm_uvs, g_params.texel_size);
    const vec2 closest_vel = textureLod(g_velocity_tex, closest_frag.xy, 0.0).xy;

    imageStore(g_out_dilated_depth_img, icoord, vec4(closest_frag.z, 0.0, 0.0, 0.0));
    imageStore(g_out_dilated_velocity_img, icoord, vec4(closest_vel, 0.0, 0.0));

    //
    // Reconstruct previous depth
    //
    const vec2 reproj_uvs = norm_uvs - closest_vel * g_params.texel_size;
    const float uvx = fract(float(g_params.img_size.x) * reproj_uvs.x + 0.5);
    const float uvy = fract(float(g_params.img_size.y) * reproj_uvs.y + 0.5);

    vec4 w = vec4(1.0);
    // Bilinear weights
    w.x *= (1.0 - uvx) * (1.0 - uvy);
    w.y *= (uvx) * (1.0 - uvy);
    w.z *= (1.0 - uvx) * (uvy);
    w.w *= (uvx) * (uvy);

    const float WeightThreshold = 0.01;

    const ivec2 base_pos = ivec2(floor(vec2(g_params.img_size) * reproj_uvs - 0.5));
    if (w.x > WeightThreshold) {
        const ivec2 store_pos = base_pos + ivec2(0, 0);
        imageAtomicMax(g_out_reconstructed_depth_img, store_pos, floatBitsToUint(closest_frag.z));
    }
    if (w.y > WeightThreshold) {
        const ivec2 store_pos = base_pos + ivec2(1, 0);
        imageAtomicMax(g_out_reconstructed_depth_img, store_pos, floatBitsToUint(closest_frag.z));
    }
    if (w.z > WeightThreshold) {
        const ivec2 store_pos = base_pos + ivec2(0, 1);
        imageAtomicMax(g_out_reconstructed_depth_img, store_pos, floatBitsToUint(closest_frag.z));
    }
    if (w.w > WeightThreshold) {
        const ivec2 store_pos = base_pos + ivec2(1, 1);
        imageAtomicMax(g_out_reconstructed_depth_img, store_pos, floatBitsToUint(closest_frag.z));
    }
}
