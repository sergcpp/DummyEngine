#ifndef SSR_CLASSIFY_INTERFACE_H
#define SSR_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRClassifyTiles)

struct Params {
    uvec2 img_size;
    vec2 thresholds;
    uvec2 samples_and_guided;
    uint frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int VARIANCE_TEX_SLOT = 4;
const int RAY_COUNTER_SLOT = 5;
const int RAY_LIST_SLOT = 6;
const int TILE_LIST_SLOT = 7;
const int SOBOL_BUF_SLOT = 8;
const int SCRAMLING_TILE_BUF_SLOT = 9;
const int RANKING_TILE_BUF_SLOT = 10;

const int REFL_IMG_SLOT = 0;
const int NOISE_IMG_SLOT = 1;

INTERFACE_END

#endif // SSR_CLASSIFY_INTERFACE_H