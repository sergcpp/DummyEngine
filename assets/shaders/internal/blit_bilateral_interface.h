#ifndef BLIT_BILATERAL_INTERFACE_H
#define BLIT_BILATERAL_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Bilateral)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE resolution;
    VEC2_TYPE _pad;
    float vertical;
};

DEF_CONST_INT(DEPTH_TEX_SLOT, 0)
DEF_CONST_INT(INPUT_TEX_SLOT, 1);

INTERFACE_END

#endif // BLIT_BILATERAL_INTERFACE_H