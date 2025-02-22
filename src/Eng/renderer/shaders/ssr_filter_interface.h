#ifndef SSR_FILTER_INTERFACE_H
#define SSR_FILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRFilter)

struct Params {
    vec4 rotator;
    uvec2 img_size;
    uvec2 frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 3;
const int SPEC_TEX_SLOT = 4;
const int NORM_TEX_SLOT = 5;
const int REFL_TEX_SLOT = 6;
const int AVG_REFL_TEX_SLOT = 7;
const int SAMPLE_COUNT_TEX_SLOT = 8;
const int VARIANCE_TEX_SLOT = 9;
const int TILE_LIST_BUF_SLOT = 10;

const int OUT_DENOISED_IMG_SLOT = 0;
const int OUT_AVG_REFL_IMG_SLOT = 1;
const int OUT_VARIANCE_IMG_SLOT = 2;

INTERFACE_END

#endif // SSR_FILTER_INTERFACE_H