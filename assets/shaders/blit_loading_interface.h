#ifndef BLIT_LOADING_INTERFACE_H
#define BLIT_LOADING_INTERFACE_H

#include "internal/_interface_common.h"

INTERFACE_START(Loading)

struct Params {
    vec4 transform;
    vec2 texel_size;
    float time;
    float fade;
};

INTERFACE_END

#endif // BLIT_LOADING_INTERFACE_H