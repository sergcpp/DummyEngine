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

const int TLAS_SLOT = 1;
const int NOISE_TEX_SLOT = 2;
const int DEPTH_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int GEO_DATA_BUF_SLOT = 5;
const int MATERIAL_BUF_SLOT = 6;
const int VTX_BUF1_SLOT = 7;
const int NDX_BUF_SLOT = 9;
const int TILE_LIST_SLOT = 10;
const int BLAS_BUF_SLOT = 11;
const int TLAS_BUF_SLOT = 12;
const int PRIM_NDX_BUF_SLOT = 13;
const int MESHES_BUF_SLOT = 14;
const int MESH_INSTANCES_BUF_SLOT = 15;

const int OUT_SHADOW_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOWS_INTERFACE_H