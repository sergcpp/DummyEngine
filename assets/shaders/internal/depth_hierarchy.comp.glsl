#version 310 es

#include "_cs_common.glsl"
#include "depth_hierarchy_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

layout(binding = DEPTH_TEX_SLOT) uniform highp sampler2D depth_texture;
layout(binding = DEPTH_IMG_SLOT, r32f) uniform image2D depth_hierarchy[6];

layout(local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

ivec2 limit_coords(ivec2 icoord) {
	return clamp(icoord, ivec2(0), params.depth_size.xy - ivec2(1));
}

float ReduceSrcDepth4(ivec2 base) {
	float v0 = texelFetch(depth_texture, limit_coords(base + ivec2(0, 0)), 0).r;
	float v1 = texelFetch(depth_texture, limit_coords(base + ivec2(0, 1)), 0).r;
	float v2 = texelFetch(depth_texture, limit_coords(base + ivec2(1, 0)), 0).r;
	float v3 = texelFetch(depth_texture, limit_coords(base + ivec2(1, 1)), 0).r;
	return LinearizeDepth(min(min(v0, v1), min(v2, v3)), params.clip_info);
}

void WriteDstDepth(int index, ivec2 icoord, float v) {
	imageStore(depth_hierarchy[index], icoord, vec4(v));
}

shared float g_group_shared_depth[16][16];

float ReduceIntermediate(ivec2 i0, ivec2 i1, ivec2 i2, ivec2 i3) {
	float v0 = g_group_shared_depth[i0.x][i0.y];
	float v1 = g_group_shared_depth[i1.x][i1.y];
	float v2 = g_group_shared_depth[i2.x][i2.y];
	float v3 = g_group_shared_depth[i3.x][i3.y];
	return min(min(v0, v1), min(v2, v3));
}

void main() {
	//
	// Taken from https://github.com/GPUOpen-Effects/FidelityFX-SPD
	//

    // Copy the first level and linearize it
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 8; ++j) {
			ivec2 icoord = ivec2(2 * gl_GlobalInvocationID.x + i, 8 * gl_GlobalInvocationID.y + j);
			if (icoord.x < params.depth_size.x && icoord.y < params.depth_size.y) {
				float depth_val = LinearizeDepth(texelFetch(depth_texture, icoord, 0).r, params.clip_info);
				imageStore(depth_hierarchy[0], icoord, vec4(depth_val));
			}
		}
	}
	
	//
	// Remap index for easier reduction
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
		
		for (int i = 0; i < 4; ++i) {
			g_group_shared_depth[x][y] = v[i];
			barrier();
			if (gl_LocalInvocationIndex < 64) {
				v[i] = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
										  ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
				WriteDstDepth(2, ivec2(gl_WorkGroupID.xy * 16) + ivec2(x + (i % 2) * 8, y + (i / 2) * 8), v[i]);
			}
			barrier();
		}
		
		if (gl_LocalInvocationIndex < 64) {
			g_group_shared_depth[x + 0][y + 0] = v[0];
			g_group_shared_depth[x + 8][y + 0] = v[1];
			g_group_shared_depth[x + 0][y + 8] = v[2];
			g_group_shared_depth[x + 8][y + 8] = v[3];
		}
		barrier();
	}
	
	{ // Init mip level 3
		if (gl_LocalInvocationIndex < 64) {
			float v = ReduceIntermediate(ivec2(x * 2 + 0, y * 2 + 0), ivec2(x * 2 + 1, y * 2 + 0),
										 ivec2(x * 2 + 0, y * 2 + 1), ivec2(x * 2 + 1, y * 2 + 1));
			WriteDstDepth(3, ivec2(gl_WorkGroupID.xy * 8) + ivec2(x, y), v);
			// store to LDS, try to reduce bank conflicts
			// x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
			// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
			// 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0 x
			// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
			// x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
			// ...
			// x 0 x 0 x 0 x 0 x 0 x 0 x 0 x 0
			g_group_shared_depth[x * 2 + y % 2][y * 2] = v;
		}
		barrier();
	}
	
	{ // Init mip level 4
		if (gl_LocalInvocationIndex < 16) {
			// x 0 x 0
			// 0 0 0 0
			// 0 x 0 x
			// 0 0 0 0
			float v = ReduceIntermediate(ivec2(x * 4 + 0 + 0, y * 4 + 0),
			                             ivec2(x * 4 + 2 + 0, y * 4 + 0),
										 ivec2(x * 4 + 0 + 1, y * 4 + 2),
										 ivec2(x * 4 + 2 + 1, y * 4 + 2));
			WriteDstDepth(4, ivec2(gl_WorkGroupID.xy * 4) + ivec2(x, y), v);
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
			g_group_shared_depth[x * 4 + y][y * 4] = v;
		}
		barrier();
	}
	
	{ // Init mip level 5
		if (gl_LocalInvocationIndex < 4) {
			// x 0 0 0 x 0 0 0
			// ...
			// 0 x 0 0 0 x 0 0
			float v = ReduceIntermediate(ivec2(x * 8 + 0 + 0 + y * 2, y * 8 + 0),
										 ivec2(x * 8 + 4 + 0 + y * 2, y * 8 + 0),
									     ivec2(x * 8 + 0 + 1 + y * 2, y * 8 + 4),
										 ivec2(x * 8 + 4 + 1 + y * 2, y * 8 + 4));
			WriteDstDepth(5, ivec2(gl_WorkGroupID.xy * 2) + ivec2(x, y), v);
			// store to LDS
			// x x x x 0 ...
			// 0 ...
			g_group_shared_depth[x + y * 2][0] = v;
		}
		barrier();
	}
}
