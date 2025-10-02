#ifndef RT_SHADOW_FILTER_INTERFACE_H
#define RT_SHADOW_FILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowFilter)

struct Params {
    uvec2 img_size;
    vec2 inv_img_size;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int INPUT_TEX_SLOT = 4;
const int TILE_METADATA_BUF_SLOT = 5;

const int OUT_RESULT_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOW_FILTER_INTERFACE_H