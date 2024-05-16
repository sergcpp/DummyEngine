#ifndef BLIT_DOWN_INTERFACE_H
#define BLIT_DOWN_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DownColor)

struct Params {
    vec4 transform;
    vec4 resolution;
};

const int SRC_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_DOWN_INTERFACE_H