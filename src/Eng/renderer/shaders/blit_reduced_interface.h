#ifndef BLIT_REDUCED_INTERFACE_H
#define BLIT_REDUCED_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Reduced)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE offset;
};

const int SRC_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_REDUCED_INTERFACE_H