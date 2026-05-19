#ifndef SAMPLE_LIGHTS_INTERFACE_H
#define SAMPLE_LIGHTS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SampleLights)

struct Params {
    uvec2 img_size;
    uint lights_count;
    uint frame_index;
};

struct Params2 {
    uint volume_index;
    uint stoch_lights_count;
    uint pass_hash;
    uint oct_index;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint GRP_SIZE2_X = 64;

const uint TLAS_SLOT = 2;
const uint RANDOM_SEQ_BUF_SLOT = 3;
const uint ALBEDO_TEX_SLOT = 4;
const uint DEPTH_TEX_SLOT = 5;
const uint NORM_TEX_SLOT = 6;
const uint SPEC_TEX_SLOT = 7;
const uint GEO_DATA_BUF_SLOT = 8;
const uint MATERIAL_BUF_SLOT = 9;
const uint VTX_BUF1_SLOT = 10;
const uint NDX_BUF_SLOT = 11;
const uint TILE_LIST_SLOT = 12;
const uint BLAS_BUF_SLOT = 13;
const uint TLAS_BUF_SLOT = 14;
const uint PRIM_NDX_BUF_SLOT = 15;
const uint MESH_INSTANCES_BUF_SLOT = 16;
const uint LIGHTS_BUF_SLOT = 17;
const uint LIGHT_NODES_BUF_SLOT = 18;
const uint OFFSET_TEX_SLOT = 4; // overlaps with ALBEDO_TEX_SLOT

const uint OUT_DIFFUSE_IMG_SLOT = 0;
const uint OUT_SPECULAR_IMG_SLOT = 1;
const uint OUT_SH1_DATA_BUF_SLOT = 1;

INTERFACE_END

#endif // SAMPLE_LIGHTS_INTERFACE_H