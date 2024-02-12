#ifndef BLIT_UPSCALE_INTERFACE_H
#define BLIT_UPSCALE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Upscale)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE resolution;
    VEC4_TYPE clip_info;
};

const int DEPTH_TEX_SLOT = 0;
const int DEPTH_LOW_TEX_SLOT = 1;
const int INPUT_TEX_SLOT = 2;

INTERFACE_END

#endif // BLIT_UPSCALE_INTERFACE_H