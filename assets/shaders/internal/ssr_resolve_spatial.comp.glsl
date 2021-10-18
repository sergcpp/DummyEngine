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
#include "ssr_resolve_spatial_interface.glsl"

/*
UNIFORM_BLOCKS
    UniformParams : $ubUnifParamLoc
*/

LAYOUT_PARAMS uniform UniformParams {
    Params params;
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D depth_texture;
layout(binding = NORM_TEX_SLOT) uniform sampler2D normal_texture;
layout(binding = ROUGH_TEX_SLOT) uniform sampler2D rough_texture;
layout(binding = REFL_TEX_SLOT) uniform sampler2D refl_texture;

layout(std430, binding = TILE_METADATA_MASK_SLOT) readonly buffer TileMetadataMask {
    uint g_tile_metadata_mask[];
};

layout(binding = OUT_DENOISED_IMG_SLOT, r11f_g11f_b10f) uniform image2D out_denoised_img;

layout (local_size_x = LOCAL_GROUP_SIZE_X, local_size_y = LOCAL_GROUP_SIZE_Y, local_size_z = 1) in;

shared uint g_shared_0[16][16];
shared uint g_shared_1[16][16];
shared uint g_shared_2[16][16];
shared uint g_shared_3[16][16];
shared float g_shared_depth[16][16];

vec3 LoadRadianceFromSharedMemory(ivec2 idx) {
    return vec3(unpackHalf2x16(g_shared_0[idx.y][idx.x]),
                unpackHalf2x16(g_shared_1[idx.y][idx.x]).x);
}

vec3 LoadNormalFromSharedMemory(ivec2 idx) {
    return vec3(unpackHalf2x16(g_shared_2[idx.y][idx.x]),
                unpackHalf2x16(g_shared_3[idx.y][idx.x]).x);
}

float LoadDepthFromSharedMemory(ivec2 idx) {
    return g_shared_depth[idx.y][idx.x];
}

void StoreInSharedMemory(ivec2 idx, vec3 radiance, vec3 normal, float depth) {
    g_shared_0[idx.y][idx.x] = packHalf2x16(radiance.xy);
    g_shared_1[idx.y][idx.x] = packHalf2x16(vec2(radiance.z, 0));
    g_shared_2[idx.y][idx.x] = packHalf2x16(normal.xy);
    g_shared_3[idx.y][idx.x] = packHalf2x16(vec2(normal.z, 0));
    g_shared_depth[idx.y][idx.x] = depth;
}

void LoadWithOffset(ivec2 dispatch_thread_id, ivec2 _offset, out vec3 radiance, out vec3 normal, out float depth) {
    dispatch_thread_id += _offset;
    radiance = texelFetch(refl_texture, dispatch_thread_id, 0).rgb;
    normal = texelFetch(normal_texture, dispatch_thread_id, 0).xyz;
    depth = texelFetch(depth_texture, dispatch_thread_id, 0).r;
}

void StoreWithOffset(ivec2 group_thread_id, ivec2 _offset, vec3 radiance, vec3 normal, float depth) {
    group_thread_id += _offset;
    StoreInSharedMemory(group_thread_id, radiance, normal, depth);
}

void InitSharedMemory(ivec2 dispatch_thread_id, ivec2 group_thread_id) {
    // Load 16x16 region into shared memory.
    ivec2 offset_0 = ivec2(0, 0);
    ivec2 offset_1 = ivec2(8, 0);
    ivec2 offset_2 = ivec2(0, 8);
    ivec2 offset_3 = ivec2(8, 8);
    
    /* mediump */ vec3 radiance_0;
    /* mediump */ vec3 normal_0;
    float depth_0;

    /* mediump */ vec3 radiance_1;
    /* mediump */ vec3 normal_1;
    float depth_1;

    /* mediump */ vec3 radiance_2;
    /* mediump */ vec3 normal_2;
    float depth_2;

    /* mediump */ vec3 radiance_3;
    /* mediump */ vec3 normal_3;
    float depth_3;

    /// XA
    /// BC

    dispatch_thread_id -= 4; // 1 + 3 => additional band + left band
    LoadWithOffset(dispatch_thread_id, offset_0, radiance_0, normal_0, depth_0); // X
    LoadWithOffset(dispatch_thread_id, offset_1, radiance_1, normal_1, depth_1); // A
    LoadWithOffset(dispatch_thread_id, offset_2, radiance_2, normal_2, depth_2); // B
    LoadWithOffset(dispatch_thread_id, offset_3, radiance_3, normal_3, depth_3); // C

    StoreWithOffset(group_thread_id, offset_0, radiance_0, normal_0, depth_0); // X
    StoreWithOffset(group_thread_id, offset_1, radiance_1, normal_1, depth_1); // A
    StoreWithOffset(group_thread_id, offset_2, radiance_2, normal_2, depth_2); // B
    StoreWithOffset(group_thread_id, offset_3, radiance_3, normal_3, depth_3); // C
}

bool IsGlossyReflection(float roughness) {
    return roughness < params.thresholds.x;
}

bool IsMirrorReflection(float roughness) {
    return roughness < params.thresholds.y; //0.0001;
}

float Gaussian(float x, float m, float sigma) {
    float a = length(x - m) / sigma;
    a *= a;
    return exp(-0.5 * a);
}

vec3 Resolve(ivec2 group_thread_id, vec3 center_radiance, vec3 center_normal, float depth_sigma, float center_depth) {
    vec3 accumulated_radiance = center_radiance;
    float accumulated_weight = 1.0;

    const float normal_sigma = 64.0;

    // First 15 numbers of Halton(2,3) streteched to [-3,3]
    const ivec2 reuse_offsets[] = {
        ivec2(0, 1),
        ivec2(-2, 1),
        ivec2(2, -3),
        ivec2(-3, 0),
        ivec2(1, 2),
        ivec2(-1, -2),
        ivec2(3, 0),
        ivec2(-3, 3),
        ivec2(0, -3),
        ivec2(-1, -1),
        ivec2(2, 1),
        ivec2(-2, -2),
        ivec2(1, 0),
        ivec2(0, 2),
        ivec2(3, -1)
    };
    const uint sample_count = 15;

    int mirror = 2 * ((group_thread_id.x + group_thread_id.y) % 2) - 1;

    for (int i = 0; i < sample_count; ++i) {
        ivec2 new_idx = group_thread_id + mirror * reuse_offsets[i];
        vec3 normal = LoadNormalFromSharedMemory(new_idx);
        float depth = LoadDepthFromSharedMemory(new_idx);
        vec3 radiance = LoadRadianceFromSharedMemory(new_idx);
        float weight = 1.0
            * GetEdgeStoppingNormalWeight(center_normal, normal, normal_sigma)
            * Gaussian(center_depth, depth, depth_sigma)
            ;

        // Accumulate all contributions.
        accumulated_weight += weight;
        accumulated_radiance += weight * radiance.xyz;
    }

    accumulated_radiance /= max(accumulated_weight, 0.00001);
    return accumulated_radiance;
}

void ResolveSpatial(ivec2 dispatch_thread_id, ivec2 group_thread_id, uvec2 screen_size) {
    uint tile_meta_data_index = GetTileMetaDataIndex(dispatch_thread_id, screen_size.x);
    tile_meta_data_index = subgroupBroadcastFirst(tile_meta_data_index);
    bool needs_denoiser = g_tile_metadata_mask[tile_meta_data_index] != 0u;
    
    if (needs_denoiser) {
        float center_roughness = texelFetch(rough_texture, dispatch_thread_id, 0).r;
        InitSharedMemory(dispatch_thread_id, group_thread_id);
        
        groupMemoryBarrier();
        barrier();
        
        group_thread_id += 4; // center threads in shared memory
        vec3 center_radiance = LoadRadianceFromSharedMemory(group_thread_id);
        
        if (!IsGlossyReflection(center_roughness) || IsMirrorReflection(center_roughness)) {
            imageStore(out_denoised_img, dispatch_thread_id, vec4(center_radiance, 1.0));
            return;
        }
        
        vec3 center_normal = LoadNormalFromSharedMemory(group_thread_id);
        float center_depth = LoadDepthFromSharedMemory(group_thread_id);
        
        vec3 resolved_radiance = Resolve(group_thread_id, center_radiance, center_normal, DepthSigma, center_depth);
        imageStore(out_denoised_img, dispatch_thread_id, vec4(resolved_radiance, 1.0));
    } else {
        vec3 radiance = texelFetch(refl_texture, dispatch_thread_id, 0).rgb;
        imageStore(out_denoised_img, dispatch_thread_id, vec4(radiance, 1.0));
    }
}


void main() {
    uvec2 group_id = gl_WorkGroupID.xy;
    uint group_index = gl_LocalInvocationIndex;
    uvec2 group_thread_id = RemapLane8x8(group_index);
    uvec2 dispatch_thread_id = group_id * 8 + group_thread_id;

    ResolveSpatial(ivec2(dispatch_thread_id), ivec2(group_thread_id), params.img_size.xy);
}
