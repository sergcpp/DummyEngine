#version 310 es

#include "ssr_write_indir_rt_dispatch_interface.glsl"

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};
layout(std430, binding = INDIR_ARGS_SLOT) buffer IndirArgs {
    uint g_intersect_args[];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    uint ray_count = g_ray_counter[2];
    
    g_intersect_args[0] = ray_count;
    g_intersect_args[1] = 1;
    g_intersect_args[2] = 1;

    g_ray_counter[2] = 0;
    g_ray_counter[3] = ray_count;
}
