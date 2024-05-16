#ifndef BLIT_SSAO_INTERFACE_H
#define BLIT_SSAO_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSAO)

struct Params {
    vec4 transform;
    vec2 resolution;
};

const int DEPTH_TEX_SLOT = 0;
const int RAND_TEX_SLOT = 1;

INTERFACE_END

#endif // BLIT_SSAO_INTERFACE_H