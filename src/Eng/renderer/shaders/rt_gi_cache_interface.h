#ifndef RT_GI_CACHE_INTERFACE_H
#define RT_GI_CACHE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTGICache)

struct Params {
    int volume_index;
    int stoch_lights_count;
    int _pad1;
    int _pad2;
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

const int LOCAL_GROUP_SIZE_X = 64;

const float TEX_LOD_OFFSET = 4;

const int TLAS_SLOT = 1;
const int ENV_TEX_SLOT = 2;
const int GEO_DATA_BUF_SLOT = 3;
const int MATERIAL_BUF_SLOT = 4;
const int PRIM_NDX_BUF_SLOT = 5;
const int MESHES_BUF_SLOT = 6;
const int MESH_INSTANCES_BUF_SLOT = 7;
const int BLAS_BUF_SLOT = 8;
const int TLAS_BUF_SLOT = 9;
const int LIGHTS_BUF_SLOT = 10;
const int VTX_BUF1_SLOT = 11;
const int NDX_BUF_SLOT = 12;
const int SHADOW_TEX_SLOT = 13;
const int LTC_LUTS_TEX_SLOT = 14;
const int RANDOM_SEQ_BUF_SLOT = 15;
const int STOCH_LIGHTS_BUF_SLOT = 16;
const int LIGHT_NODES_BUF_SLOT = 17;
const int CELLS_BUF_SLOT = 18;
const int ITEMS_BUF_SLOT = 19;
const int IRRADIANCE_TEX_SLOT = 20;
const int DISTANCE_TEX_SLOT = 21;
const int OFFSET_TEX_SLOT = 22;

const int OUT_RAY_DATA_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_GI_CACHE_INTERFACE_H