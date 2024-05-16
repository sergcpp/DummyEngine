#ifndef BLIT_REDUCED_INTERFACE_H
#define BLIT_REDUCED_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Reduced)

struct Params {
    vec4 transform;
    vec4 offset;
};

const int SRC_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_REDUCED_INTERFACE_H