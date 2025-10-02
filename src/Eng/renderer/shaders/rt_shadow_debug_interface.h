#ifndef RT_SHADOW_DEBUG_INTERFACE_H
#define RT_SHADOW_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowDebug)

struct Params {
    uvec2 img_size;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 4;

const int HIT_MASK_TEX_SLOT = 1;
const int OUT_RESULT_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOW_DEBUG_INTERFACE_H