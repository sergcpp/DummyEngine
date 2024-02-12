#ifndef RT_SHADOW_CLASSIFY_TILES_INTERFACE_H
#define RT_SHADOW_CLASSIFY_TILES_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowClassifyTiles)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE inv_img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 2;
const int VELOCITY_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int HISTORY_TEX_SLOT = 5;
const int PREV_DEPTH_TEX_SLOT = 6;
const int PREV_MOMENTS_TEX_SLOT = 7;
const int RAY_HITS_BUF_SLOT = 8;
const int OUT_TILE_METADATA_BUF_SLOT = 9;

const int OUT_REPROJ_RESULTS_IMG_SLOT = 0;
const int OUT_MOMENTS_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_SHADOW_CLASSIFY_TILES_INTERFACE_H