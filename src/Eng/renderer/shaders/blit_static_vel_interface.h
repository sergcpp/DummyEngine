#ifndef BLIT_STATIC_VEL_INTERFACE_H
#define BLIT_STATIC_VEL_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(BlitStaticVel)

struct Params {
    VEC4_TYPE transform;
};

const int DEPTH_TEX_SLOT = 0;

INTERFACE_END

#endif // BLIT_STATIC_VEL_INTERFACE_H