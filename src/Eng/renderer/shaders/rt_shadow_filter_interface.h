#ifndef RT_SHADOW_FILTER_INTERFACE_H
#define RT_SHADOW_FILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowFilter)

struct Params {
    uvec2 img_size;
    vec2 inv_img_size;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 2;
const uint NORM_TEX_SLOT = 3;
const uint INPUT_TEX_SLOT = 4;
const uint TILE_METADATA_BUF_SLOT = 5;

const uint OUT_RESULT_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOW_FILTER_INTERFACE_H