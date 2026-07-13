#version 450 core
#extension GL_EXT_control_flow_attributes : require

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"
#include "rad_cache_common.glsl"

#include "rt_gi_cache_interface.h"

#pragma multi_compile _ PARTIAL

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(std430, binding = RAY_HITS_BUF_SLOT) readonly buffer RayHitsList {
    uint g_ray_hits[];
};

layout(binding = OFFSET_TEX_SLOT) uniform sampler2DArray g_offset_tex;

layout(std430, binding = INOUT_ENTRIES_BUF_SLOT) buffer InOutEntries {
    uint g_inout_entries[];
};

layout(std430, binding = INOUT_VOXELS_BUF_SLOT) buffer InOutVoxels {
    uint g_inout_voxels[];
};

layout(std430, binding = RAY_COUNTER_BUF_SLOT) buffer RayCounter {
    uint g_inout_ray_counter[];
};

layout(std430, binding = INOUT_RAY_IDS_BUF_SLOT) buffer RayIDs {
    uint g_inout_ray_ids[];
};

layout(std430, binding = OUT_ACTIVE_BUF_SLOT) writeonly buffer OutActive {
    uint g_out_active[];
};

bool hash_map_insert(const uint hash_key, out uint cache_entry) {
    const uint base_slot = hash_map_base_slot(hash_key);
    for (uint bucket_offset = 0; bucket_offset < HASH_GRID_HASH_MAP_BUCKET_SIZE; ++bucket_offset) {
        const uint prev_hash_key = atomicCompSwap(g_inout_entries[base_slot + bucket_offset], HASH_GRID_INVALID_HASH_KEY, hash_key);
        if (prev_hash_key == HASH_GRID_INVALID_HASH_KEY || prev_hash_key == hash_key) {
            cache_entry = base_slot + bucket_offset;
            return true;
        }
    }
    return false;
}

uint insert_entry(const vec3 p, const bool backfacing, const vec3 cam_pos) {
    const uint hash_key = compute_hash(p, backfacing, cam_pos);
    uint cache_entry = HASH_GRID_INVALID_CACHE_ENTRY;
    hash_map_insert(hash_key, cache_entry);
    return cache_entry;
}

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint ray_index = gl_GlobalInvocationID.x;
    const uint probe_plane_index = gl_GlobalInvocationID.y;
    const uint plane_index = gl_GlobalInvocationID.z;
    if (ray_index < PROBE_FIXED_RAYS_COUNT) {
        return;
    }

    const uint noscroll_probe_index = (plane_index * PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z) + probe_plane_index;

    const uvec3 probe_coords = get_probe_coords(noscroll_probe_index);
    const uint probe_index = get_scrolling_probe_index(probe_coords, g_params.grid_scroll.xyz);

    const uvec3 tex_coords = get_probe_texel_coords(probe_index, g_params.volume_index);
    const bool is_inactive = ray_index >= PROBE_FIXED_RAYS_COUNT && texelFetch(g_offset_tex, ivec3(tex_coords), 0).w < 0.5;
#ifdef PARTIAL
    const uvec3 oct_index = get_probe_coords(probe_index) & 1u;
    const bool is_wrong_oct = (oct_index.x | (oct_index.y << 1u) | (oct_index.z << 2u)) != g_params.oct_index;
#else
    const bool is_wrong_oct = false;
#endif

    if (!IsScrollingPlaneProbe(probe_index, g_params.grid_scroll.xyz, g_params.grid_scroll_diff.xyz) && (is_inactive || is_wrong_oct)) {
        return;
    }

    const vec3 probe_pos = get_probe_pos_ws(g_params.volume_index, probe_coords, g_params.grid_scroll.xyz, g_params.grid_origin.xyz, g_params.grid_spacing.xyz, g_offset_tex);
    const vec3 probe_ray_dir = get_probe_ray_dir(ray_index, g_params.quat_rot);
    const uvec3 output_coords = get_ray_data_coords(ray_index, probe_index);

    const uint read_offset = (PROBE_TOTAL_RAYS_COUNT * probe_index + ray_index) * RAY_HITS_STRIDE;
    const uint hit_data0 = g_ray_hits[read_offset + 0];
    const uint hit_data2 = g_ray_hits[read_offset + 2];

    // TODO: Separate shader invocations (only hits update the cache)!!!
    if (hit_data2 == 0xffffffffu) {
        // No hit, nothing to update
        return;
    }

    const uint obj_index = hit_data0 >> 16u;
    const vec2 inter_uv = unpackUnorm2x16(g_ray_hits[read_offset + 3]);

    float hit_t = uintBitsToFloat(g_ray_hits[read_offset + 1]);
    const bool backfacing = (hit_t < 0.0);
    hit_t = abs(hit_t);

    const vec3 P = probe_pos + probe_ray_dir * hit_t;

    const uint cache_entry = insert_entry(P, backfacing, g_shrd_data.cam_pos_and_exp.xyz);
    if (cache_entry != HASH_GRID_INVALID_CACHE_ENTRY) {
        const uint frame_new = packHalf2x16(vec2(0.0, float(g_params.pass_hash)));
        const uint frame_old = atomicExchange(g_inout_voxels[2 * cache_entry + 1], frame_new);
        if (unpackHalf2x16(frame_old).y != float(g_params.pass_hash)) {
            const uint out_index = atomicAdd(g_inout_ray_counter[0], 1);
            g_out_active[out_index] = cache_entry;
        }
        uint out_data = PROBE_TOTAL_RAYS_COUNT * noscroll_probe_index + ray_index;
        // Prefer closer probe
        out_data |= clamp(uint(0.5 * hit_t / length(g_params.grid_spacing.xyz)), 0u, 255u) << 24u;
        atomicMin(g_inout_ray_ids[cache_entry], out_data);
    }
}