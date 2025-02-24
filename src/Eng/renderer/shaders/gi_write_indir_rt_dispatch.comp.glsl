#version 430 core

#include "gi_write_indir_rt_dispatch_interface.h"

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = INDIR_ARGS_SLOT) writeonly buffer IndirArgs {
    uint g_intersect_args[];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_count = g_ray_counter[6];

    // raytracing pipeline dispatch
    g_intersect_args[0] = ray_count;
    g_intersect_args[1] = 1;
    g_intersect_args[2] = 1;

    // compute pipeline dispatch (for inlined raytracing)
    g_intersect_args[3] = (ray_count + 63) / 64;
    g_intersect_args[4] = 1;
    g_intersect_args[5] = 1;

    g_ray_counter[6] = 0;
    g_ray_counter[7] = ray_count;
}
