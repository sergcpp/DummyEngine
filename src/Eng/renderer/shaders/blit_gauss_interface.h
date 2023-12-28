#ifndef BLIT_GAUSS_INTERFACE_H
#define BLIT_GAUSS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Gauss)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE vertical;
};

DEF_CONST_INT(SRC_TEX_SLOT, 0)

INTERFACE_END

#endif // BLIT_GAUSS_INTERFACE_H