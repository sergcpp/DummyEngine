#ifndef FOG_INTERFACE_H
#define FOG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Fog)

struct Params {
    uvec4 froxel_res;
    uvec2 img_res;
    int thread_offset;
    float anisotropy;
    vec4 scatter_color; // w is absorption
    vec4 emission_color; // w is density
    vec4 bbox_min;
    vec4 bbox_max;
    int frame_index;
    float hist_weight;
    int _pad2[2];
};

const uint GRP_SIZE_2D_X = 8;
const uint GRP_SIZE_2D_Y = 8;

const uint GRP_SIZE_3D_X = 4;
const uint GRP_SIZE_3D_Y = 4;
const uint GRP_SIZE_3D_Z = 4;

const uint SHADOW_DEPTH_TEX_SLOT = 2;
const uint SHADOW_COLOR_TEX_SLOT = 3;
const uint FR_EMISSION_TEX_SLOT = 4;
const uint FR_SCATTER_TEX_SLOT = 5;
const uint DEPTH_TEX_SLOT = 6;
const uint STBN_TEX_SLOT = 7;

const uint LIGHT_BUF_SLOT = 8;
const uint DECAL_BUF_SLOT = 9;
const uint CELLS_BUF_SLOT = 10;
const uint ITEMS_BUF_SLOT = 11;
const uint ENVMAP_TEX_SLOT = 12;

const uint IRRADIANCE_TEX_SLOT = 13;
const uint DISTANCE_TEX_SLOT = 14;
const uint OFFSET_TEX_SLOT = 15;

const uint GEO_DATA_BUF_SLOT = 8;
const uint MATERIAL_BUF_SLOT = 9;
const uint TLAS_SLOT = 10;
const uint BLAS_BUF_SLOT = 11;
const uint TLAS_BUF_SLOT = 12;
const uint PRIM_NDX_BUF_SLOT = 13;
const uint MESH_INSTANCES_BUF_SLOT = 14;

const uint VTX_BUF1_SLOT = 15;
const uint NDX_BUF_SLOT = 16;

const uint OUT_FR_EMISSION_IMG_SLOT = 0;
const uint OUT_FR_SCATTER_IMG_SLOT = 1;
const uint OUT_FR_FINAL_IMG_SLOT = 0;

INTERFACE_END

#endif // FOG_INTERFACE_H