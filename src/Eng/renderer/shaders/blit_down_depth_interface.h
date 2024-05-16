#ifndef BLIT_DOWN_DEPTH_INTERFACE_H
#define BLIT_DOWN_DEPTH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DownDepth)

struct Params {
    vec4 transform;
    vec4 clip_info;
    float linearize;
};

const int DEPTH_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_DOWN_DEPTH_INTERFACE_H