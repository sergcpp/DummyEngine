#ifndef RT_SHADOWS_INTERFACE_H
#define RT_SHADOWS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadows)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    uint frame_index;
};

struct RayPayload {
    vec3 col;
    float cone_width;
};

const uint TLAS_SLOT = 1;
const uint NOISE_TEX_SLOT = 2;
const uint DEPTH_TEX_SLOT = 3;
const uint NORM_TEX_SLOT = 4;
const uint GEO_DATA_BUF_SLOT = 5;
const uint MATERIAL_BUF_SLOT = 6;
const uint VTX_BUF1_SLOT = 7;
const uint NDX_BUF_SLOT = 9;
const uint TILE_LIST_SLOT = 10;
const uint BLAS_BUF_SLOT = 11;
const uint TLAS_BUF_SLOT = 12;
const uint PRIM_NDX_BUF_SLOT = 13;
const uint MESH_INSTANCES_BUF_SLOT = 14;

const uint OUT_SHADOW_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOWS_INTERFACE_H