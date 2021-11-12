#ifndef BLIT_SSR_INTERFACE
#define BLIT_SSR_INTERFACE

#include "_interface_common.glsl"

INTERFACE_START(SSRTrace)

struct Params {
    VEC4_TYPE transform;
};

DEF_CONST_INT(DEPTH_TEX_SLOT, 0)
DEF_CONST_INT(NORM_TEX_SLOT, 1)

INTERFACE_END

#endif // BLIT_SSR_INTERFACE