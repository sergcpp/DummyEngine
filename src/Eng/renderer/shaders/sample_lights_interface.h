#ifndef SAMPLE_LIGHTS_INTERFACE_H
#define SAMPLE_LIGHTS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SampleLights)

struct Params {
    uvec2 img_size;
    uint lights_count;
    uint frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int TLAS_SLOT = 2;
const int RANDOM_SEQ_BUF_SLOT = 3;
const int ALBEDO_TEX_SLOT = 4;
const int DEPTH_TEX_SLOT = 5;
const int NORM_TEX_SLOT = 6;
const int SPEC_TEX_SLOT = 7;
const int GEO_DATA_BUF_SLOT = 8;
const int MATERIAL_BUF_SLOT = 9;
const int VTX_BUF1_SLOT = 10;
const int NDX_BUF_SLOT = 11;
const int TILE_LIST_SLOT = 12;
const int BLAS_BUF_SLOT = 13;
const int TLAS_BUF_SLOT = 14;
const int PRIM_NDX_BUF_SLOT = 15;
const int MESHES_BUF_SLOT = 16;
const int MESH_INSTANCES_BUF_SLOT = 17;
const int LIGHTS_BUF_SLOT = 18;
const int LIGHT_NODES_BUF_SLOT = 19;

const int OUT_DIFFUSE_IMG_SLOT = 0;
const int OUT_SPECULAR_IMG_SLOT = 1;

INTERFACE_END

#endif // SAMPLE_LIGHTS_INTERFACE_H