#ifndef RT_REFLECTIONS_INTERFACE_H
#define RT_REFLECTIONS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTReflections)

struct Params {
    VEC4_TYPE grid_origin;
    IVEC4_TYPE grid_scroll;
    VEC4_TYPE grid_spacing;
    UVEC2_TYPE img_size;
    float pixel_spread_angle;
    float _pad[1];
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

const int TLAS_SLOT = 2;
const int DEPTH_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int ENV_TEX_SLOT = 5;
const int GEO_DATA_BUF_SLOT = 6;
const int MATERIAL_BUF_SLOT = 7;
const int VTX_BUF1_SLOT = 8;
const int VTX_BUF2_SLOT = 9;
const int NDX_BUF_SLOT = 10;
const int PRIM_NDX_BUF_SLOT = 11;
const int MESHES_BUF_SLOT = 12;
const int MESH_INSTANCES_BUF_SLOT = 13;
const int RAY_COUNTER_SLOT = 14;
const int RAY_LIST_SLOT = 15;
const int BLAS_BUF_SLOT = 16;
const int TLAS_BUF_SLOT = 17;
const int NOISE_TEX_SLOT = 18;
const int LIGHTS_BUF_SLOT = 19;
const int SHADOW_TEX_SLOT = 20;
const int LTC_LUTS_TEX_SLOT = 21;
const int CELLS_BUF_SLOT = 22;
const int IRRADIANCE_TEX_SLOT = 25;
const int DISTANCE_TEX_SLOT = 26;
const int OFFSET_TEX_SLOT = 27;
const int ITEMS_BUF_SLOT = 28;

const int OUT_REFL_IMG_SLOT = 0;
const int OUT_RAYLEN_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_REFLECTIONS_INTERFACE_H