#ifndef RT_SHADOW_CLASSIFY_INTERFACE_H
#define RT_SHADOW_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowClassify)

struct Params {
    uvec2 img_size;
    uint frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 4;

const int DEPTH_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int TILE_COUNTER_SLOT = 5;
const int TILE_LIST_SLOT = 6;
const int BN_PMJ_SEQ_BUF_SLOT = 7;

const int OUT_RAY_HITS_IMG_SLOT = 0;
const int OUT_NOISE_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_SHADOW_CLASSIFY_INTERFACE_H