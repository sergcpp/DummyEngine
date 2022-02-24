#ifndef BLIT_DOWN_INTERFACE_GLSL
#define BLIT_DOWN_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(DownColor)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE resolution;
};

DEF_CONST_INT(SRC_TEX_SLOT, 0)

INTERFACE_END

#endif // BLIT_DOWN_INTERFACE_GLSL