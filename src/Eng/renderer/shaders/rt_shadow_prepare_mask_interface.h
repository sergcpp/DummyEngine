#ifndef RT_SHADOW_PREPARE_MASK_INTERFACE_H
#define RT_SHADOW_PREPARE_MASK_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTShadowPrepareMask)

struct Params {
    uvec2 img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 4;

const int HIT_MASK_TEX_SLOT = 1;

const int SHADOW_MASK_BUF_SLOT = 0;

INTERFACE_END

#endif // RT_SHADOW_PREPARE_MASK_INTERFACE_H