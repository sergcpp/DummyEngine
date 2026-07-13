#version 430 core

#include "_cs_common.glsl"
#include "rad_cache_common.glsl"
#include "debug_rad_cache_interface.h"

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout(std430, binding = CACHE_ENTRIES_BUF_SLOT) readonly buffer CacheEntries {
    uint g_cache_entries[];
};

layout(std430, binding = CACHE_VOXELS_BUF_SLOT) readonly buffer CacheVoxels {
    uint g_cache_voxels[];
};

layout(binding = DEPTH_TEX_SLOT) uniform sampler2D g_depth_tex;
layout(binding = NORM_TEX_SLOT) uniform usampler2D g_normal_tex;

layout(binding = OUT_IMG_SLOT, rgba8) uniform restrict writeonly image2D g_out_img;

bool hash_map_find(const uint hash_key, inout uint cache_entry, out uint bucket_offset) {
    const uint base_slot = hash_map_base_slot(hash_key);
    for (bucket_offset = 0; bucket_offset < HASH_GRID_HASH_MAP_BUCKET_SIZE; ++bucket_offset) {
        const uint stored_hash_key = g_cache_entries[base_slot + bucket_offset];
        if (stored_hash_key == hash_key) {
            cache_entry = base_slot + bucket_offset;
            return true;
        }
    }
    return false;
}

uint find_entry(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uint hash_key = compute_hash(p, backfacing, cam_pos);
    uint cache_entry = HASH_GRID_INVALID_CACHE_ENTRY, collisions_count;
    hash_map_find(hash_key, cache_entry, collisions_count);
    return cache_entry;
}

layout (local_size_x = GRP_SIZE_X, local_size_y = GRP_SIZE_Y, local_size_z = 1) in;

void main() {
    const uvec2 px_coords = gl_GlobalInvocationID.xy;
    if (px_coords.x >= g_params.img_size.x || px_coords.y >= g_params.img_size.y) {
        return;
    }

    const uint linear_index = px_coords.y * g_params.img_size.x + px_coords.x;
    if (linear_index < HASH_GRID_CACHE_ENTRIES_COUNT) {
        const uint hash = g_cache_entries[linear_index];
        if (hash != HASH_GRID_INVALID_HASH_KEY) {
            imageStore(g_out_img, ivec2(px_coords), vec4(GetColorFromHash32(hash_jenkins32(hash)), 1.0));
        } else {
            imageStore(g_out_img, ivec2(px_coords), vec4(0.0, 0.0, 0.0, 1.0));
        }
        return;
    }

    const vec2 norm_uvs = (vec2(px_coords) + 0.5) / vec2(g_params.img_size);
    const uvec2 ucoord = uvec2(norm_uvs * g_shrd_data.fren_res.xy);

    const float depth = texelFetch(g_depth_tex, ivec2(ucoord), 0).x;
    if (depth == 0.0) {
        imageStore(g_out_img, ivec2(px_coords), vec4(0, 0, 0, 1));
        return;
    }
    const float lin_depth = LinearizeDepth(depth, g_shrd_data.clip_info);
    const vec4 pos_cs = vec4(2.0 * norm_uvs - 1.0, depth, 1.0);
    const vec3 pos_ws = TransformFromClipSpace(g_shrd_data.world_from_clip, pos_cs);

    const vec4 normal_ws = UnpackNormalAndRoughness(texelFetch(g_normal_tex, ivec2(ucoord), 0).x);
    const vec3 view_ray_vs = normalize(pos_ws - g_shrd_data.cam_pos_and_exp.xyz);

    const uint cache_entry = find_entry(pos_ws, false, g_shrd_data.cam_pos_and_exp.xyz);
    if (cache_entry != HASH_GRID_INVALID_CACHE_ENTRY) {
        const vec3 debug_color = hash_grid_debug(pos_ws, false, g_shrd_data.cam_pos_and_exp.xyz);
        imageStore(g_out_img, ivec2(px_coords), vec4(debug_color, 1.0));
    } else {
        imageStore(g_out_img, ivec2(px_coords), vec4(0.0, 0.0, 0.0, 1.0));
    }
}