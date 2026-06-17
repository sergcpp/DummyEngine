#ifndef RT_SHADOW_CLASSIFY_INTERFACE_H
#define RT_SHADOW_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowClassify)

struct Params {
    uvec2 img_size;
    uint frame_index;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 4;

const uint DEPTH_TEX_SLOT = 2;
const uint NORM_TEX_SLOT = 3;
const uint TILE_COUNTER_SLOT = 5;
const uint TILE_LIST_SLOT = 6;
const uint BN_PMJ_SEQ_BUF_SLOT = 7;

const uint OUT_RAY_HITS_IMG_SLOT = 0;
const uint OUT_NOISE_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_SHADOW_CLASSIFY_INTERFACE_H