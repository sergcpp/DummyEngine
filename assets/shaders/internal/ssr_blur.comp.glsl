#version 310 es
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_ARB_shading_language_packing : require

#if defined(GL_ES) || defined(VULKAN)
    precision highp int;
    precision highp float;
#endif

#include "_common.glsl"
#include "ssr_common.glsl"
#include "ssr_blur_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

layout(binding = ROUGH_TEX_SLOT) uniform sampler2D rough_texture;
layout(binding = REFL_TEX_SLOT) uniform sampler2D refl_texture;

layout(std430, binding = TILE_METADATA_MASK_SLOT) readonly buffer TileMetadataMask {
    uint g_tile_metadata_mask[];
};

layout(binding = OUT_DENOISED_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_denoised_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

bool IsGlossyReflection(float roughness) {
    return roughness < params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < params.thresholds.y; //0.0001;
}

shared uint g_shared_0[12][12];
shared uint g_shared_1[12][12];

void LoadFromSharedMemory(ivec2 idx, out /* mediump */ vec3 radiance, out /* mediump */ float roughness) {
	uvec2 tmp0;
	tmp0.x = g_shared_0[idx.x][idx.y];
	tmp0.y = g_shared_1[idx.x][idx.y];

	/* mediump */ vec4 tmp1 = vec4(unpackHalf2x16(tmp0.x), unpackHalf2x16(tmp0.y));
	radiance = tmp1.xyz;
	roughness = tmp1.w;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 offset, out /* mediump */ vec3 radiance, out /* mediump */ float roughness) {
    dispatch_thread_id += offset;
    radiance = texelFetch(refl_texture, dispatch_thread_id, 0).rgb;
    roughness = texelFetch(rough_texture, dispatch_thread_id, 0).r;
}

void StoreWithOffset(ivec2 group_thread_id, ivec2 offset, /* mediump */ vec3 radiance, /* mediump */ float roughness) {
    group_thread_id += offset;
	g_shared_0[group_thread_id.x][group_thread_id.y] = packHalf2x16(radiance.rg);
	g_shared_1[group_thread_id.x][group_thread_id.y] = packHalf2x16(vec2(radiance.b, roughness));
}

void InitializeGroupSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id) {
    ivec2 offset_0 = ivec2(0);
    if (group_thread_id.x < 4) {
        offset_0 = ivec2(8, 0);
    } else if (group_thread_id.y >= 4) {
        offset_0 = ivec2(4, 4);
    } else {
        offset_0 = -group_thread_id; // map all threads to the same memory location to guarantee cache hits.
    }

    ivec2 offset_1 = ivec2(0);
    if (group_thread_id.y < 4) {
        offset_1 = ivec2(0, 8);
    } else {
        offset_1 = -group_thread_id; // map all threads to the same memory location to guarantee cache hits.
    }

    /* mediump */ vec3 radiance_0;
    /* mediump */ float roughness_0;

    /* mediump */ vec3 radiance_1;
    /* mediump */ float roughness_1;

    /* mediump */ vec3 radiance_2;
    /* mediump */ float roughness_2;

    // XXA
    // XXA
    // BBC

    dispatch_thread_id -= 2;
    LoadWithOffset(dispatch_thread_id, ivec2(0, 0), radiance_0, roughness_0); // X
    LoadWithOffset(dispatch_thread_id, offset_0, radiance_1, roughness_1); // A & C
    LoadWithOffset(dispatch_thread_id, offset_1, radiance_2, roughness_2); // B
    
    StoreWithOffset(group_thread_id, ivec2(0, 0), radiance_0, roughness_0); // X
    if (group_thread_id.x < 4 || group_thread_id.y >= 4) {
        StoreWithOffset(group_thread_id, offset_0, radiance_1, roughness_1); // A & C
    }
    if (group_thread_id.y < 4) {
        StoreWithOffset(group_thread_id, offset_1, radiance_2, roughness_2); // B
    }
}

float GaussianWeight(int x, int y) {
    uint weights[] = { 6, 4, 1 };
    return float(weights[abs(x)] * weights[abs(y)]) / 256.0;
}

vec3 Resolve(ivec2 group_thread_id, /* mediump */ float center_roughness, /* mediump */ float roughness_sigma_min, /* mediump */ float roughness_sigma_max) {
    /* mediump */ vec3 sum = vec3(0.0);
    /* mediump */ float total_weight = 0.0;

    const int Radius = 2;
    for (int dy = -Radius; dy <= Radius; ++dy) {
        for (int dx = -Radius; dx <= Radius; ++dx) {
            ivec2 texel_coords = group_thread_id + ivec2(dx, dy);

            /* mediump */ vec3 radiance;
            /* mediump */ float roughness;
            LoadFromSharedMemory(texel_coords, radiance, roughness);

            /* mediump */ float weight = 1.0
                * GaussianWeight(dx, dy)
                * GetEdgeStoppingRoughnessWeight(center_roughness, roughness, roughness_sigma_min, roughness_sigma_max);
            sum += weight * radiance;
            total_weight += weight;
        }
    }

    sum /= max(total_weight, 0.0001);
    return sum;
}

void Blur(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size) {
    // First check if we have to denoise or if a simple copy is enough
    uint tile_meta_data_index = GetTileMetaDataIndex(dispatch_thread_id, screen_size.x);
    tile_meta_data_index = subgroupBroadcastFirst(tile_meta_data_index);
    bool needs_denoiser = g_tile_metadata_mask[tile_meta_data_index] != 0u;

    if (needs_denoiser) {
        InitializeGroupSharedMemory(dispatch_thread_id, group_thread_id);
        
		groupMemoryBarrier();
		barrier();

        group_thread_id += 2; // Center threads in shared memory

        /* mediump */ vec3 center_radiance;
        /* mediump */ float center_roughness;
        LoadFromSharedMemory(group_thread_id, center_radiance, center_roughness);

        if (!IsGlossyReflection(center_roughness) || IsMirrorReflection(center_roughness)) {
            return;
        }

        /* mediump */ vec3 radiance = Resolve(group_thread_id, center_roughness, RoughnessSigmaMin, RoughnessSigmaMax);
		imageStore(out_denoised_img, dispatch_thread_id, vec4(radiance, 1.0));
    }
}

void main() {
	Blur(ivec2(gl_GlobalInvocationID.xy), ivec2(gl_LocalInvocationID.xy), params.img_size.xy);
}
