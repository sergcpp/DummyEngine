#ifndef RT_GI_INTERFACE_H
#define RT_GI_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTGI)

struct Params {
    UVEC2_TYPE img_size;
    float pixel_spread_angle;
    UINT_TYPE frame_index;
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

const int TLAS_SLOT = 0;
const int DEPTH_TEX_SLOT = 1;
const int NORM_TEX_SLOT = 2;
const int ENV_TEX_SLOT = 4;
const int GEO_DATA_BUF_SLOT = 5;
const int MATERIAL_BUF_SLOT = 6;
const int VTX_BUF1_SLOT = 7;
const int VTX_BUF2_SLOT = 8;
const int NDX_BUF_SLOT = 9;
const int RAY_COUNTER_SLOT = 10;
const int RAY_LIST_SLOT = 11;
const int NOISE_TEX_SLOT = 12;
const int LMAP_TEX_SLOTS = 13;
const int OUT_GI_IMG_SLOT = 18;

INTERFACE_END

#endif // RT_GI_INTERFACE_H