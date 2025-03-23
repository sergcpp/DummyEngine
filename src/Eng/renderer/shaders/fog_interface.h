#ifndef FOG_INTERFACE_H
#define FOG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Fog)

struct Params {
    ivec4 froxel_res;
    ivec2 img_res;
    float density;
    float anisotropy;
    int frame_index;
    float hist_weight;
    int _pad[2];
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int SHADOW_DEPTH_TEX_SLOT = 2;
const int SHADOW_COLOR_TEX_SLOT = 3;
const int FROXELS_TEX_SLOT = 4;
const int DEPTH_TEX_SLOT = 5;
const int RANDOM_SEQ_BUF_SLOT = 6;

const int OUT_FROXELS_IMG_SLOT = 0;
const int INOUT_COLOR_IMG_SLOT = 1;

INTERFACE_END

#endif // FOG_INTERFACE_H