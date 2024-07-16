#ifndef BLIT_OIT_DEPTH_INTERFACE_H
#define BLIT_OIT_DEPTH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(BlitOITDepth)

struct Params {
    ivec2 img_size;
    ivec2 _pad0;
    int layer_index;
};

const int OIT_DEPTH_BUF_SLOT = 0;

INTERFACE_END

#endif // BLIT_OIT_DEPTH_INTERFACE_H