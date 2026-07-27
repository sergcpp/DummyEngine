#version 430 core
#extension GL_EXT_control_flow_attributes : require

#include "_cs_common.glsl"
#include "gi_cache_common.glsl"
#include "rad_cache_common.glsl"

#include "rt_gi_cache_interface.h"

LAYOUT_PARAMS uniform UniformParams {
    Params g_params;
};

layout (binding = BIND_UB_SHARED_DATA_BUF, std140) uniform SharedDataBlock {
    shared_data_t g_shrd_data;
};

layout(std430, binding = INOUT_ENTRIES_BUF_SLOT) buffer InOutEntries {
    uint g_inout_entries[];
};

layout(std430, binding = INOUT_VOXELS_BUF_SLOT) buffer InOutVoxels {
    uint g_inout_voxels[];
};

layout (local_size_x = GRP_SIZE_X, local_size_y = 1, local_size_z = 1) in;

void main() {
    const uint cache_entry = gl_GlobalInvocationID.x;
    if (g_inout_entries[cache_entry] == HASH_GRID_INVALID_HASH_KEY) {
        return;
    }

    const uint cached_frame = (g_inout_voxels[2 * cache_entry + 1] >> 16u);
    const uint frame_diff = (g_params.pass_hash - cached_frame) & 0xffffu;
    if (frame_diff > RAD_CACHE_STALE_FRAME_COUNT_MAX) {
        g_inout_voxels[2 * cache_entry + 0] = 0;
        g_inout_voxels[2 * cache_entry + 1] = 0;
        g_inout_entries[cache_entry] = HASH_GRID_INVALID_HASH_KEY;
    }
}