#ifndef DEBUG_RAD_CACHE_INTERFACE_H
#define DEBUG_RAD_CACHE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DebugRadCache)

struct Params {
    uvec2 img_size;
    uint _unused0;
    uint _unused1;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint NORM_TEX_SLOT = 2;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // DEBUG_RAD_CACHE_INTERFACE_H