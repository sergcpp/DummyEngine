#ifndef DEBUG_IMAGE_INTERFACE_H
#define DEBUG_IMAGE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DebugImage)

struct Params {
    uvec2 img_size;
    uint channel;
    uint _unused;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint INPUT_TEX_SLOT = 1;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // DEBUG_IMAGE_INTERFACE_H