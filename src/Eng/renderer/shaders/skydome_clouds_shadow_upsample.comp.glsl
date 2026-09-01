#version 430 core

#include "_cs_common.glsl"
#include "skydome_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params2 g_params;
};

layout(binding = MOMENTS_B0_TEX_SLOT) uniform sampler2D g_moments_b0_tex;
layout(binding = MOMENTS_B1234_TEX_SLOT) uniform sampler2D g_moments_b1234_tex;

layout(binding = MOMENTS_B0_HIST_TEX_SLOT) uniform sampler2D g_moments_b0_hist_tex;
layout(binding = MOMENTS_B1234_HIST_TEX_SLOT) uniform sampler2D g_moments_b1234_hist_tex;

layout(binding = OUT_B0_IMG_SLOT, r16f) uniform image2D g_out_b0_img;
layout(binding = OUT_B1234_IMG_SLOT, rgba16f) uniform image2D g_out_b1234_img;

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 ucoord = gl_GlobalInvocationID.xy;

    float out_b0 = texelFetch(g_moments_b0_hist_tex, ivec2(ucoord), 0).x;
    vec4 out_b1234 = texelFetch(g_moments_b1234_hist_tex, ivec2(ucoord), 0);

    if (all(equal(ucoord % 4, g_params.sample_coord))) {
        out_b0 = texelFetch(g_moments_b0_tex, ivec2(ucoord / 4), 0).x;
        out_b1234 = texelFetch(g_moments_b1234_tex, ivec2(ucoord / 4), 0);
    }

    imageStore(g_out_b0_img, ivec2(ucoord), vec4(out_b0, 0.0, 0.0, 0.0));
    imageStore(g_out_b1234_img, ivec2(ucoord), out_b1234);
}