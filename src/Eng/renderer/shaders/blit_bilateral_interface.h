#ifndef BLIT_BILATERAL_INTERFACE_H
#define BLIT_BILATERAL_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Bilateral)

struct Params {
    vec4 transform;
    vec2 resolution;
    vec2 _pad;
    float vertical;
};

const int DEPTH_TEX_SLOT = 0;
const int INPUT_TEX_SLOT = 1;

INTERFACE_END

#endif // BLIT_BILATERAL_INTERFACE_H