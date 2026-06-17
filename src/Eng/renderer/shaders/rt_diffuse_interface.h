#ifndef RT_DIFFUSE_INTERFACE_H
#define RT_DIFFUSE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuse)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    uint frame_index;
    int lights_count;
    uint is_hwrt;
    uint _pad[2];
};

const uint RAY_HITS_STRIDE = 5;
const uint RAY_MISS_STRIDE = 2;
const uint RAY_LIST_STRIDE = 4;

const uint GRP_SIZE_X = 64;

const float TEX_LOD_OFFSET = 3;

const uint TLAS_SLOT = 2;
const uint DEPTH_TEX_SLOT = 3;
const uint NORM_TEX_SLOT = 4;
const uint ENV_TEX_SLOT = 5;
const uint GEO_DATA_BUF_SLOT = 6;
const uint MATERIAL_BUF_SLOT = 7;
const uint PRIM_NDX_BUF_SLOT = 8;
const uint MESH_INSTANCES_BUF_SLOT = 9;
const uint RAY_COUNTER_SLOT = 10;
const uint RAY_LIST_SLOT = 11;
const uint BLAS_BUF_SLOT = 12;
const uint TLAS_BUF_SLOT = 13;
const uint NOISE_TEX_SLOT = 14;
const uint LIGHTS_BUF_SLOT = 15;
const uint VTX_BUF1_SLOT = 16;
const uint NDX_BUF_SLOT = 17;
const uint SHADOW_DEPTH_TEX_SLOT = 18;
const uint SHADOW_COLOR_TEX_SLOT = 19;
const uint LTC_LUTS_TEX_SLOT = 20;
const uint CELLS_BUF_SLOT = 21;
const uint ITEMS_BUF_SLOT = 22;
const uint IRRADIANCE_TEX_SLOT = 25;
const uint DISTANCE_TEX_SLOT = 26;
const uint OFFSET_TEX_SLOT = 27;
const uint STOCH_LIGHTS_BUF_SLOT = 28;
const uint LIGHT_NODES_BUF_SLOT = 29;
const uint RAY_HITS_BUF_SLOT = 30;

const uint OUT_GI_IMG_SLOT = 0;
const uint OUT_RAY_HITS_BUF_SLOT = 1;
const uint OUT_RAY_LIST_BUF_SLOT = 1;

INTERFACE_END

#endif // RT_DIFFUSE_INTERFACE_H