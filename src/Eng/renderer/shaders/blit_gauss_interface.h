#ifndef BLIT_GAUSS_INTERFACE_H
#define BLIT_GAUSS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Gauss)

struct Params {
    vec4 transform;
    vec4 vertical;
};

const int SRC_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_GAUSS_INTERFACE_H