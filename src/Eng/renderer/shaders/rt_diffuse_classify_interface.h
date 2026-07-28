#ifndef RT_DIFFUSE_CLASSIFY_INTERFACE_H
#define RT_DIFFUSE_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuseClassifyTiles)

struct Params {
    uvec2 img_size;
    vec2 thresholds;
    uvec2 samples_and_guided;
    uint frame_index;
    uint tile_count;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint SPEC_TEX_SLOT = 2;
const uint VARIANCE_TEX_SLOT = 3;
const uint RAY_COUNTER_SLOT = 4;
const uint RAY_LIST_SLOT = 5;
const uint TILE_LIST_SLOT = 6;

const uint OUT_GI_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_DIFFUSE_CLASSIFY_INTERFACE_H