#version 430 core

#include "_cs_common.glsl"
#include "motion_blur_interface.h"

#pragma multi_compile _ VERTICAL

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = VELOCITY_TEX_SLOT) uniform sampler2D g_velocity_tex;

layout(binding = OUT_IMG_SLOT, rgba16f) uniform restrict writeonly image2D g_out_img;

shared vec3 g_shared_0[GRP_SIZE + TILE_RES];

#ifndef VERTICAL
layout (local_size_x = GRP_SIZE, local_size_y = 1, local_size_z = 1) in;
#else
layout (local_size_x = 1, local_size_y = GRP_SIZE, local_size_z = 1) in;
#endif

void main() {
    const ivec2 icoord = ivec2(gl_GlobalInvocationID.xy);
    if (icoord.x >= g_params.img_size.x || icoord.y >= g_params.img_size.y) {
        return;
    }

#ifndef VERTICAL
    vec2 fetch = texelFetch(g_velocity_tex, icoord, 0).xy;
    g_shared_0[gl_LocalInvocationIndex] = vec3(fetch, length2(fetch));
    fetch = texelFetch(g_velocity_tex, min(icoord + ivec2(TILE_RES, 0), ivec2(g_params.img_size) - 1), 0).xy;
    g_shared_0[gl_LocalInvocationIndex + TILE_RES] = vec3(fetch, length2(fetch));

    if ((icoord.x % TILE_RES) != 0) {
        return;
    }
    const ivec2 out_coord = ivec2(icoord.x / TILE_RES, icoord.y);
#else
    g_shared_0[gl_LocalInvocationIndex] = texelFetch(g_velocity_tex, icoord, 0).xyz;
    g_shared_0[gl_LocalInvocationIndex + TILE_RES] = texelFetch(g_velocity_tex, min(icoord + ivec2(0, TILE_RES), ivec2(g_params.img_size) - 1), 0).xyz;

    if ((icoord.y % TILE_RES) != 0) {
        return;
    }
    const ivec2 out_coord = ivec2(icoord.x, icoord.y / TILE_RES);
#endif

    groupMemoryBarrier(); barrier();

    vec3 out_vel = g_shared_0[gl_LocalInvocationIndex];
    float max_vel_sq = out_vel.z;
    for (int i = 1; i < TILE_RES; ++i) {
        const vec3 vel = g_shared_0[gl_LocalInvocationIndex + i];
        out_vel.z = min(out_vel.z, vel.z);
        if (vel.z > max_vel_sq) {
            out_vel.xy = vel.xy;
            max_vel_sq = vel.z;
        }
    }
    imageStore(g_out_img, out_coord, vec4(out_vel, 0.0));
}
