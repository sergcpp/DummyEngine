#ifndef RT_DEBUG_INTERFACE_H
#define RT_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDebug)

struct Params {
    UVEC2_TYPE img_size;
    float pixel_spread_angle;
    UINT_TYPE root_node;
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int TLAS_SLOT = 1;
const int ENV_TEX_SLOT = 2;
const int GEO_DATA_BUF_SLOT = 3;
const int MATERIAL_BUF_SLOT = 4;
const int VTX_BUF1_SLOT = 5;
const int VTX_BUF2_SLOT = 6;
const int NDX_BUF_SLOT = 7;
const int BLAS_BUF_SLOT = 8;
const int TLAS_BUF_SLOT = 9;
const int PRIM_NDX_BUF_SLOT = 10;
const int MESHES_BUF_SLOT = 11;
const int MESH_INSTANCES_BUF_SLOT = 12;
const int LIGHTS_BUF_SLOT = 13;
const int LMAP_TEX_SLOTS = 14;
const int SHADOW_TEX_SLOT = 19;
const int LTC_LUTS_TEX_SLOT = 20;
const int CELLS_BUF_SLOT = 21;
const int ITEMS_BUF_SLOT = 22;
const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_DEBUG_INTERFACE_H