#version 430 core

#include "_common.glsl"
#include "rt_gi_cache_interface.h"

layout(std430, binding = RAY_COUNTER_BUF_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = OUT_INDIR_ARGS_BUF_SLOT) writeonly buffer IndirArgs {
    uint g_intersect_args[];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint entries_count = g_ray_counter[0];

    g_intersect_args[0] = (entries_count + 63) / 64;
    g_intersect_args[1] = 1;
    g_intersect_args[2] = 1;

    g_ray_counter[0] = 0;
    g_ray_counter[1] = entries_count;
}
