#ifndef RT_REFLECTIONS_INTERFACE_H
#define RT_REFLECTIONS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTReflections)

struct Params {
    uvec2 img_size;
    float pixel_spread_angle;
    int lights_count;
};

struct RayPayload {
    vec3 col;
    float first_roughness;
    vec3 throughput;
    int transp_depth;
    //vec3 emission;
    float hit_t;
    bool is_last_bounce;
};

const int GRP_SIZE_X = 64;

const float TEX_LOD_OFFSET_MIN = 1;
const float TEX_LOD_OFFSET_MAX = 3;

const float RECURSION_ROUGHNESS_THRES = 0.15f;

const int TLAS_SLOT = 2;
const int DEPTH_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int ENV_TEX_SLOT = 5;
const int GEO_DATA_BUF_SLOT = 6;
const int MATERIAL_BUF_SLOT = 7;
const int PRIM_NDX_BUF_SLOT = 8;
const int MESH_INSTANCES_BUF_SLOT = 9;
const int RAY_COUNTER_SLOT = 10;
const int RAY_LIST_SLOT = 11;
const int BLAS_BUF_SLOT = 12;
const int TLAS_BUF_SLOT = 13;
const int NOISE_TEX_SLOT = 14;
const int OIT_DEPTH_BUF_SLOT = 14;
const int LIGHTS_BUF_SLOT = 15;
const int VTX_BUF1_SLOT = 16;
const int VTX_BUF2_SLOT = 17;
const int NDX_BUF_SLOT = 18;
const int SHADOW_DEPTH_TEX_SLOT = 19;
const int SHADOW_COLOR_TEX_SLOT = 20;
const int LTC_LUTS_TEX_SLOT = 21;
const int CELLS_BUF_SLOT = 22;
const int IRRADIANCE_TEX_SLOT = 25;
const int DISTANCE_TEX_SLOT = 26;
const int OFFSET_TEX_SLOT = 27;
const int ITEMS_BUF_SLOT = 28;
const int STOCH_LIGHTS_BUF_SLOT = 29;
const int LIGHT_NODES_BUF_SLOT = 30;

const int OUT_REFL_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_REFLECTIONS_INTERFACE_H