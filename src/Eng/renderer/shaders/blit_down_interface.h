#ifndef BLIT_DOWN_INTERFACE_H
#define BLIT_DOWN_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DownColor)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE resolution;
};

const int SRC_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_DOWN_INTERFACE_H