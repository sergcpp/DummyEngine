#ifndef BLIT_OIT_DEPTH_INTERFACE_H
#define BLIT_OIT_DEPTH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(BlitOITDepth)

struct Params {
    uvec2 img_size;
    uvec2 _pad0;
    uint layer_index;
};

const uint OIT_DEPTH_BUF_SLOT = 0;

INTERFACE_END

#endif // BLIT_OIT_DEPTH_INTERFACE_H