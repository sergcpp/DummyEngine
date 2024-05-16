#ifndef RT_GI_CACHE_INTERFACE_H
#define RT_GI_CACHE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTGICache)

struct Params {
    vec4 grid_origin;
    ivec4 grid_scroll;
    vec4 grid_spacing;
    //float pixel_spread_angle;
    //uint frame_index;
};

struct RayPayload {
    vec3 col;
    float cone_width;
};

const int LOCAL_GROUP_SIZE_X = 64;

const int TLAS_SLOT = 1;
const int ENV_TEX_SLOT = 4;
const int GEO_DATA_BUF_SLOT = 5;
const int MATERIAL_BUF_SLOT = 6;
const int VTX_BUF1_SLOT = 7;
const int VTX_BUF2_SLOT = 8;
const int NDX_BUF_SLOT = 9;
const int PRIM_NDX_BUF_SLOT = 10;
const int MESHES_BUF_SLOT = 11;
const int MESH_INSTANCES_BUF_SLOT = 12;
const int BLAS_BUF_SLOT = 15;
const int TLAS_BUF_SLOT = 16;
const int LIGHTS_BUF_SLOT = 18;
const int SHADOW_TEX_SLOT = 19;
const int LTC_LUTS_TEX_SLOT = 20;
const int CELLS_BUF_SLOT = 21;
const int ITEMS_BUF_SLOT = 22;
const int IRRADIANCE_TEX_SLOT = 24;
const int DISTANCE_TEX_SLOT = 25;
const int OFFSET_TEX_SLOT = 26;

const int OUT_RAY_DATA_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_GI_CACHE_INTERFACE_H