#ifndef FOG_INTERFACE_H
#define FOG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Fog)

struct Params {
    ivec4 froxel_res;
    ivec2 img_res;
    float _pad;
    float anisotropy;
    vec4 scatter_color; // w is absorption
    vec4 emission_color; // w is density
    vec4 bbox_min;
    vec4 bbox_max;
    int frame_index;
    float hist_weight;
    int _pad2[2];
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int SHADOW_DEPTH_TEX_SLOT = 2;
const int SHADOW_COLOR_TEX_SLOT = 3;
const int FR_EMISSION_TEX_SLOT = 4;
const int FR_SCATTER_TEX_SLOT = 5;
const int DEPTH_TEX_SLOT = 6;
const int STBN_TEX_SLOT = 7;

const int LIGHT_BUF_SLOT = 8;
const int DECAL_BUF_SLOT = 9;
const int CELLS_BUF_SLOT = 10;
const int ITEMS_BUF_SLOT = 11;
const int ENVMAP_TEX_SLOT = 12;

const int IRRADIANCE_TEX_SLOT = 13;
const int DISTANCE_TEX_SLOT = 14;
const int OFFSET_TEX_SLOT = 15;

const int GEO_DATA_BUF_SLOT = 8;
const int MATERIAL_BUF_SLOT = 9;
const int TLAS_SLOT = 10;
const int BLAS_BUF_SLOT = 11;
const int TLAS_BUF_SLOT = 12;
const int PRIM_NDX_BUF_SLOT = 13;
const int MESH_INSTANCES_BUF_SLOT = 14;

const int VTX_BUF1_SLOT = 15;
const int NDX_BUF_SLOT = 16;

const int OUT_FR_EMISSION_IMG_SLOT = 0;
const int OUT_FR_SCATTER_IMG_SLOT = 1;
const int OUT_FR_FINAL_IMG_SLOT = 0;

INTERFACE_END

#endif // FOG_INTERFACE_H