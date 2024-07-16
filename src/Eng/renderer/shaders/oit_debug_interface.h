#ifndef OIT_DEBUG_INTERFACE_H
#define OIT_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(OITDebug)

struct Params {
    ivec2 img_size;
    ivec2 _pad0;
    int layer_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int OIT_DEPTH_BUF_SLOT = 1;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // OIT_DEBUG_INTERFACE_H