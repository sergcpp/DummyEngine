#ifndef OIT_DEBUG_INTERFACE_H
#define OIT_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(OITDebug)

struct Params {
    uvec2 img_size;
    uvec2 _pad0;
    uint layer_index;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint OIT_DEPTH_BUF_SLOT = 1;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // OIT_DEBUG_INTERFACE_H