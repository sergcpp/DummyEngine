#ifndef RT_SHADOW_CLASSIFY_TILES_INTERFACE_H
#define RT_SHADOW_CLASSIFY_TILES_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowClassifyTiles)

struct Params {
    uvec2 img_size;
    vec2 inv_img_size;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 2;
const uint VELOCITY_TEX_SLOT = 3;
const uint NORM_TEX_SLOT = 4;
const uint HISTORY_TEX_SLOT = 5;
const uint PREV_DEPTH_TEX_SLOT = 6;
const uint PREV_MOMENTS_TEX_SLOT = 7;
const uint RAY_HITS_BUF_SLOT = 8;
const uint OUT_TILE_METADATA_BUF_SLOT = 9;

const uint OUT_REPROJ_RESULTS_IMG_SLOT = 0;
const uint OUT_MOMENTS_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_SHADOW_CLASSIFY_TILES_INTERFACE_H