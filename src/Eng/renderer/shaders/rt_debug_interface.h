#ifndef RT_DEBUG_INTERFACE_H
#define RT_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDebug)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    uint root_node;
    uint cull_mask;
    uint _pad[3];
};

struct RayPayload {
    vec3 col;
    float cone_width;
    vec3 throughput;
    float throughput_dist;
    float closest_dist;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint TLAS_SLOT = 1;
const uint ENV_TEX_SLOT = 2;
const uint GEO_DATA_BUF_SLOT = 3;
const uint MATERIAL_BUF_SLOT = 4;
const uint VTX_BUF1_SLOT = 5;
const uint VTX_BUF2_SLOT = 6;
const uint NDX_BUF_SLOT = 7;
const uint BLAS_BUF_SLOT = 8;
const uint TLAS_BUF_SLOT = 9;
const uint PRIM_NDX_BUF_SLOT = 10;
const uint MESH_INSTANCES_BUF_SLOT = 11;
const uint LIGHTS_BUF_SLOT = 12;
const uint IRRADIANCE_TEX_SLOT = 13;
const uint DISTANCE_TEX_SLOT = 14;
const uint OFFSET_TEX_SLOT = 15;
const uint SHADOW_DEPTH_TEX_SLOT = 16;
const uint SHADOW_COLOR_TEX_SLOT = 17;
const uint LTC_LUTS_TEX_SLOT = 18;
const uint CELLS_BUF_SLOT = 19;
const uint ITEMS_BUF_SLOT = 20;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_DEBUG_INTERFACE_H