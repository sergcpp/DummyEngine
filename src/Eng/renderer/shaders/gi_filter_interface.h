#ifndef GI_FILTER_INTERFACE_H
#define GI_FILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GIFilter)

struct Params {
    vec4 rotator;
    uvec2 img_size;
    uvec2 frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int SPEC_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int GI_TEX_SLOT = 4;
const int SAMPLE_COUNT_TEX_SLOT = 5;
const int VARIANCE_TEX_SLOT = 6;
const int TILE_LIST_BUF_SLOT = 7;

const int OUT_DENOISED_IMG_SLOT = 0;

INTERFACE_END

#endif // GI_FILTER_INTERFACE_H