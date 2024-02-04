#version 310 es
#ifndef NO_SUBGROUP_EXTENSIONS
#extension GL_KHR_shader_subgroup_quad : enable
#endif

#if defined(GL_ES) || defined(VULKAN) || defined(GL_SPIRV)
    precision highp int;
    precision highp float;
#endif

#include "_cs_common.glsl"
#include "depth_hierarchy_interface.h"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
PERM @MIPS_7
PERM @NO_SUBGROUP_EXTENSIONS
PERM @MIPS_7;NO_SUBGROUP_EXTENSIONS
*/

#if !defined(NO_SUBGROUP_EXTENSIONS) && !defined(GL_KHR_shader_subgroup_quad)
#define NO_SUBGROUP_EXTENSIONS
#endif

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D g_depth_tex;
layout(std430, binding = ATOMIC_CNT_SLOT) buffer AtomicCounter {
    uint g_atomic_counter;
};

#ifdef MIPS_7 // simplified version
layout(binding = DEPTH_IMG_SLOT, r32f) uniform image2D g_depth_hierarchy[7];
#else
layout(binding = DEPTH_IMG_SLOT, r32f) uniform image2D g_depth_hierarchy[13];
#endif

layout(local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

ivec2 limit_coords(ivec2 icoord) {
    return clamp(icoord, ivec2(0), g_params.depth_size.xy - 1);
}

#define REDUCE_OP min

float ReduceSrcDepth4(ivec2 base) {
    float v0 = texelFetch(g_depth_tex, limit_coords(base + ivec2(0, 0)), 0).r;
    float v1 = texelFetch(g_depth_tex, limit_coords(base + ivec2(0, 1)), 0).r;
    float v2 = texelFetch(g_depth_tex, limit_coords(base + ivec2(1, 0)), 0).r;
    float v3 = texelFetch(g_depth_tex, limit_coords(base + ivec2(1, 1)), 0).r;
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

#if !defined(NO_SUBGROUP_EXTENSIONS)
float ReduceQuad(float v) {
    float v0 = v;
    float v1 = subgroupQuadSwapHorizontal(v);
    float v2 = subgroupQuadSwapVertical(v);
    float v3 = subgroupQuadSwapDiagonal(v);
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}
#endif

void WriteDstDepth(int index, ivec2 icoord, float v) {
    imageStore(g_depth_hierarchy[index], icoord, vec4(v));
}

shared float g_shared_depth[16][16];
shared uint g_shared_counter;

float ReduceIntermediate(ivec2 i0, ivec2 i1, ivec2 i2, ivec2 i3) {
    float v0 = g_shared_depth[i0.x][i0.y];
    float v1 = g_shared_depth[i1.x][i1.y];
    float v2 = g_shared_depth[i2.x][i2.y];
    float v3 = g_shared_depth[i3.x][i3.y];
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

float ReduceLoad4(ivec2 base) {
    float v0 = imageLoad(g_depth_hierarchy[6], base + ivec2(0, 0)).r;
    float v1 = imageLoad(g_depth_hierarchy[6], base + ivec2(0, 1)).r;
    float v2 = imageLoad(g_depth_hierarchy[6], base + ivec2(1, 0)).r;
    float v3 = imageLoad(g_depth_hierarchy[6], base + ivec2(1, 1)).r;
    return REDUCE_OP(REDUCE_OP(v0, v1), REDUCE_OP(v2, v3));
}

void DownsampleNext4Levels(int base_level, int levels_total, uvec2 work_group_id, uint x, uint y) {
    if (levels_total <= base_level + 1) return;
    { // Init mip level 3 or
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 64) {
            float v = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
                                         ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
            WriteDstDepth(base_level + 1, ivec2(work_group_id * 8) + ivec2(x, y), v);
            // store to LDS, try to reduce bank conflicts
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0 x
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            // ...
            // x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
            g_shared_depth[x * 2 + y % 2][y * 2] = v;
        }
#else
        float v = ReduceQuad(g_shared_depth[x][y]);
        // quad index 0 stores result
        if ((gl_LocalInvocationIndex % 4) == 0) {
            WriteDstDepth(base_level + 1, ivec2(work_group_id * 8) + ivec2(x / 2, y / 2), v);
            g_shared_depth[x + (y / 2) % 2][y] = v;
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 2) return;
    { // Init mip level 4
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 16) {
            // x 0 x 0
            // 0 0 0 0
            // 0 x 0 x
            // 0 0 0 0
            float v = ReduceIntermediate(ivec2(x * 4 + 0 + 0, y * 4 + 0),
                                         ivec2(x * 4 + 2 + 0, y * 4 + 0),
                                         ivec2(x * 4 + 0 + 1, y * 4 + 2),
                                         ivec2(x * 4 + 2 + 1, y * 4 + 2));
            WriteDstDepth(base_level + 2, ivec2(work_group_id * 4) + ivec2(x, y), v);
            // store to LDS
            // x 0 0 0 x 0 0 0 x 0 0 0 x 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
            // 0 x 0 0 0 x 0 0 0 x 0 0 0 x 0 0
            // ...
            // 0 0 x 0 0 0 x 0 0 0 x 0 0 0 x 0
            // ...
            // 0 0 0 x 0 0 0 x 0 0 0 x 0 0 0 x
            // ...
            g_shared_depth[x * 4 + y][y * 4] = v;
        }
#else
        if (gl_LocalInvocationIndex < 64) {
            float v = ReduceQuad(g_shared_depth[x * 2 + y % 2][y * 2]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 2, ivec2(work_group_id * 4) + ivec2(x / 2, y / 2), v);
                g_shared_depth[x * 2 + y / 2][y * 2] = v;
            }
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 3) return;
    { // Init mip level 5
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 4) {
            // x 0 0 0 x 0 0 0
            // ...
            // 0 x 0 0 0 x 0 0
            float v = ReduceIntermediate(ivec2(x * 8 + 0 + 0 + y * 2, y * 8 + 0),
                                         ivec2(x * 8 + 4 + 0 + y * 2, y * 8 + 0),
                                         ivec2(x * 8 + 0 + 1 + y * 2, y * 8 + 4),
                                         ivec2(x * 8 + 4 + 1 + y * 2, y * 8 + 4));
            WriteDstDepth(base_level + 3, ivec2(work_group_id * 2) + ivec2(x, y), v);
            // store to LDS
            // x x x x 0 ...
            // 0 ...
            g_shared_depth[x + y * 2][0] = v;
        }
#else
        if (gl_LocalInvocationIndex < 16) {
            float v = ReduceQuad(g_shared_depth[x * 4 + y][y * 4]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 3, ivec2(work_group_id * 2) + ivec2(x / 2, y / 2), v);
                g_shared_depth[x / 2 + y][0] = v;
            }
        }
#endif
        barrier();
    }
    if (levels_total <= base_level + 4) return;
    { // Init mip level 6
#if defined(NO_SUBGROUP_EXTENSIONS)
        if (gl_LocalInvocationIndex < 1) {
            // x x x x 0 ...
            // 0 ...
            float v = ReduceIntermediate(ivec2(0, 0), ivec2(1, 0), ivec2(2, 0), ivec2(3, 0));
            WriteDstDepth(base_level + 4, ivec2(work_group_id), v);
        }
#else
        if (gl_LocalInvocationIndex < 4) {
            float v = ReduceQuad(g_shared_depth[gl_LocalInvocationIndex][0]);
            // quad index 0 stores result
            if ((gl_LocalInvocationIndex % 4) == 0) {
                WriteDstDepth(base_level + 4, ivec2(work_group_id), v);
            }
        }
#endif
        barrier();
    }
}

void main() {
    //
    // Taken from https://github.com/GPUOpen-Effects/FidelityFX-SPD
    //

    // Copy the first level
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 8; ++j) {
            ivec2 icoord = ivec2(2 * gl_GlobalInvocationID.x + i, 8 * gl_GlobalInvocationID.y + j);
            float depth_val = 0.0;
            if (icoord.x < g_params.depth_size.x && icoord.y < g_params.depth_size.y) {
                depth_val = texelFetch(g_depth_tex, icoord, 0).r;
            }
            imageStore(g_depth_hierarchy[0], icoord, vec4(depth_val));
        }
    }

    int required_mips = g_params.depth_size.z;
    if (required_mips <= 1) return;

    //
    // Remap index for easier reduction (rearrange to nested quads)
    //
    //  00 01 02 03 04 05 06 07           00 01 08 09 10 11 18 19
    //  08 09 0a 0b 0c 0d 0e 0f           02 03 0a 0b 12 13 1a 1b
    //  10 11 12 13 14 15 16 17           04 05 0c 0d 14 15 1c 1d
    //  18 19 1a 1b 1c 1d 1e 1f   ---->   06 07 0e 0f 16 17 1e 1f
    //  20 21 22 23 24 25 26 27           20 21 28 29 30 31 38 39
    //  28 29 2a 2b 2c 2d 2e 2f           22 23 2a 2b 32 33 3a 3b
    //  30 31 32 33 34 35 36 37           24 25 2c 2d 34 35 3c 3d
    //  38 39 3a 3b 3c 3d 3e 3f           26 27 2e 2f 36 37 3e 3f

    uint sub_64 = uint(gl_LocalInvocationIndex % 64);
    uvec2 sub_8x8 = uvec2(bitfieldInsert(bitfieldExtract(sub_64, 2, 3), sub_64, 0, 1),
                          bitfieldInsert(bitfieldExtract(sub_64, 3, 3), bitfieldExtract(sub_64, 1, 2), 0, 2));
    uint x = sub_8x8.x + 8 * ((gl_LocalInvocationIndex / 64) % 2);
    uint y = sub_8x8.y + 8 * ((gl_LocalInvocationIndex / 64) / 2);

    { // Init mip levels 1 and 2
        float v[4];
        ivec2 icoord;

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x, y);
        v[0] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[0]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x + 16, y);
        v[1] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[1]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x, y + 16);
        v[2] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[2]);

        icoord = ivec2(gl_WorkGroupID.xy * 32) + ivec2(x + 16, y + 16);
        v[3] = ReduceSrcDepth4(icoord * 2);
        WriteDstDepth(1, icoord, v[3]);

        if (required_mips <= 2) return;

#if defined(NO_SUBGROUP_EXTENSIONS)
        for (int i = 0; i < 4; ++i) {
            g_shared_depth[x][y] = v[i];
            barrier();
            if (gl_LocalInvocationIndex < 64) {
                v[i] = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
                                          ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
                WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x + (i % 2) * 8, y + (i / 2) * 8), v[i]);
            }
            barrier();
        }

        if (gl_LocalInvocationIndex < 64) {
            g_shared_depth[x + 0][y + 0] = v[0];
            g_shared_depth[x + 8][y + 0] = v[1];
            g_shared_depth[x + 0][y + 8] = v[2];
            g_shared_depth[x + 8][y + 8] = v[3];
        }
#else
        v[0] = ReduceQuad(v[0]);
        v[1] = ReduceQuad(v[1]);
        v[2] = ReduceQuad(v[2]);
        v[3] = ReduceQuad(v[3]);

        if ((gl_LocalInvocationIndex % 4) == 0) {
            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2, y / 2), v[0]);
            g_shared_depth[x / 2][y / 2] = v[0];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2 + 8, y / 2), v[1]);
            g_shared_depth[x / 2 + 8][y / 2] = v[1];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2, y / 2 + 8), v[2]);
            g_shared_depth[x / 2][y / 2 + 8] = v[2];

            WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x / 2 + 8, y / 2 + 8), v[3]);
            g_shared_depth[x / 2 + 8][y / 2 + 8] = v[3];
        }
#endif
        barrier();
    }

    DownsampleNext4Levels(2, required_mips, gl_WorkGroupID.xy, x, y);

#ifndef MIPS_7
    if (required_mips <= 7) return;

    // Only the last active workgroup should proceed
    if (gl_LocalInvocationIndex == 0) {
        g_shared_counter = atomicAdd(g_atomic_counter, 1);
    }
    barrier();
    if (g_shared_counter != (g_params.depth_size.w - 1)) {
        return;
    }
    g_atomic_counter = 0;

    { // Init mip levels 7 and 8
        float v[4];
        ivec2 icoord;

        icoord = ivec2(x * 2 + 0, y * 2 + 0);
        v[0] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[0]);

        icoord = ivec2(x * 2 + 1, y * 2 + 0);
        v[1] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[1]);

        icoord = ivec2(x * 2 + 0, y * 2 + 1);
        v[2] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[2]);

        icoord = ivec2(x * 2 + 1, y * 2 + 1);
        v[3] = ReduceLoad4(icoord * 2);
        WriteDstDepth(7, icoord, v[3]);

        if (required_mips <= 8) return;

        float vv = REDUCE_OP(REDUCE_OP(v[0], v[1]), REDUCE_OP(v[2], v[3]));
        WriteDstDepth(8, ivec2(x, y), vv);
        g_shared_depth[x][y] = vv;
        barrier();
    }

    DownsampleNext4Levels(8, required_mips, uvec2(0, 0), x, y);
#endif
}
