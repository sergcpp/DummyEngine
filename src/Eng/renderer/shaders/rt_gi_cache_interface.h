#ifndef RT_GI_CACHE_INTERFACE_H
#define RT_GI_CACHE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTGICache)

struct Params {
    uint volume_index;
    uint stoch_lights_count;
    uint pass_hash;
    uint oct_index;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
    vec4 quat_rot;
};

struct RayPayload {
    vec3 col;
    float cone_width;
};

const uint GRP_SIZE_X = 64;

const float TEX_LOD_OFFSET = 4;

const uint RAY_HITS_STRIDE = 4;

const uint TLAS_SLOT = 4;
const uint ENV_TEX_SLOT = 5;
const uint GEO_DATA_BUF_SLOT = 6;
const uint MATERIAL_BUF_SLOT = 7;
const uint PRIM_NDX_BUF_SLOT = 8;
const uint MESH_INSTANCES_BUF_SLOT = 9;
const uint BLAS_BUF_SLOT = 10;
const uint TLAS_BUF_SLOT = 11;
const uint LIGHTS_BUF_SLOT = 12;
const uint VTX_BUF1_SLOT = 13;
const uint NDX_BUF_SLOT = 14;
const uint SHADOW_DEPTH_TEX_SLOT = 15;
const uint SHADOW_COLOR_TEX_SLOT = 16;
const uint LTC_LUTS_TEX_SLOT = 17;
const uint RANDOM_SEQ_BUF_SLOT = 18;
const uint STOCH_LIGHTS_BUF_SLOT = 19;
const uint LIGHT_NODES_BUF_SLOT = 20;
const uint CELLS_BUF_SLOT = 21;
const uint ITEMS_BUF_SLOT = 22;
const uint IRRADIANCE_TEX_SLOT = 25;
const uint DISTANCE_TEX_SLOT = 26;
const uint OFFSET_TEX_SLOT = 27;
const uint RAY_HITS_BUF_SLOT = 28;
const uint RAY_COUNTER_BUF_SLOT = 29;
const uint ACTIVE_ENTRIES_BUF_SLOT = 30;
const uint CACHE_ENTRIES_BUF_SLOT = 31;
const uint CACHE_VOXELS_BUF_SLOT = 2;
const uint RAY_IDS_BUF_SLOT = 3;

const uint OUT_RAY_HITS_BUF_SLOT = 1;
const uint INOUT_ENTRIES_BUF_SLOT = 0;
const uint INOUT_VOXELS_BUF_SLOT = 1;
const uint OUT_ACTIVE_BUF_SLOT = 2;
const uint INOUT_RAY_IDS_BUF_SLOT = 3;
const uint OUT_INDIR_ARGS_BUF_SLOT = 0;

INTERFACE_END

#endif // RT_GI_CACHE_INTERFACE_H