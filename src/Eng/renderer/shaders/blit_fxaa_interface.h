#ifndef BLIT_FXAA_INTERFACE_H
#define BLIT_FXAA_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(FXAA)

struct Params {
    vec4 transform;
    vec2 inv_resolution;
    vec2 _pad;
};

const int INPUT_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_FXAA_INTERFACE_H