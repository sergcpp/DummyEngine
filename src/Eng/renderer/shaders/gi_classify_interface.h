#ifndef GI_CLASSIFY_INTERFACE_H
#define GI_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GIClassifyTiles)

struct Params {
    uvec2 img_size;
    vec2 thresholds;
    uvec2 samples_and_guided;
    uint frame_index;
    uint tile_count;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 2;
const int SPEC_TEX_SLOT = 3;
const int VARIANCE_TEX_SLOT = 4;
const int RAY_COUNTER_SLOT = 5;
const int RAY_LIST_SLOT = 6;
const int TILE_LIST_SLOT = 7;
const int BN_PMJ_SEQ_BUF_SLOT = 8;

const int OUT_GI_IMG_SLOT = 0;
const int OUT_NOISE_IMG_SLOT = 1;

INTERFACE_END

#endif // GI_CLASSIFY_INTERFACE_H