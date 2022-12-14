#ifndef BLIT_SSAO_INTERFACE_H
#define BLIT_SSAO_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSAO)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE resolution;
};

DEF_CONST_INT(DEPTH_TEX_SLOT, 0)
DEF_CONST_INT(RAND_TEX_SLOT, 1)

INTERFACE_END

#endif // BLIT_SSAO_INTERFACE_H