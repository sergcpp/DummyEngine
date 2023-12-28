#ifndef BLIT_DOWN_DEPTH_INTERFACE_H
#define BLIT_DOWN_DEPTH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DownDepth)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE clip_info;
    float linearize;
};

DEF_CONST_INT(DEPTH_TEX_SLOT, 0)

INTERFACE_END

#endif // BLIT_DOWN_DEPTH_INTERFACE_H