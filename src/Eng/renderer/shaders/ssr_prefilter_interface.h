#ifndef SSR_PREFILTER_INTERFACE_H
#define SSR_PREFILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRPrefilter)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE thresholds;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int AVG_REFL_TEX_SLOT = 4;
const int REFL_TEX_SLOT = 5;
const int VARIANCE_TEX_SLOT = 6;
const int SAMPLE_COUNT_TEX_SLOT = 7;
const int TILE_LIST_BUF_SLOT = 8;

const int OUT_REFL_IMG_SLOT = 0;
const int OUT_VARIANCE_IMG_SLOT = 1;

INTERFACE_END

#endif // SSR_PREFILTER_INTERFACE_H