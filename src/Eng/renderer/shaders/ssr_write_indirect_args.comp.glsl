#version 430 core

#include "ssr_write_indirect_args_interface.h"

layout(std430, binding = RAY_COUNTER_SLOT) buffer RayCounter {
    uint g_ray_counter[];
};

layout(std430, binding = INDIR_ARGS_SLOT) buffer IndirArgs {
    uint g_intersect_args[];
};

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main() {
    { // intersection arguments
        const uint ray_count = g_ray_counter[0];

        g_intersect_args[0] = (ray_count + 63) / 64;
        g_intersect_args[1] = 1;
        g_intersect_args[2] = 1;

        g_ray_counter[0] = 0;
        g_ray_counter[1] = ray_count;
    }
    { // denoising arguments (1/2)
        const uint tile_count = g_ray_counter[2];

        g_intersect_args[3] = tile_count;
        g_intersect_args[4] = 1;
        g_intersect_args[5] = 1;

        g_ray_counter[2] = 0;
        g_ray_counter[3] = tile_count;
    }
    { // denoising arguments (2/2)
        const uint tile_count = g_ray_counter[4];

        g_intersect_args[6] = tile_count;
        g_intersect_args[7] = 1;
        g_intersect_args[8] = 1;

        g_ray_counter[4] = 0;
        g_ray_counter[5] = tile_count;
    }
}
