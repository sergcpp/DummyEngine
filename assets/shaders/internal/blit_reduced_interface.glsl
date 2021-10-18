#ifndef BLIT_REDUCED_INTERFACE_GLSL
#define BLIT_REDUCED_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Reduced)

struct Params {
    VEC4_TYPE transform;
    VEC4_TYPE offset;
};

#ifdef __cplusplus
    const int SRC_TEX_SLOT = 0;
#else
    #define SRC_TEX_SLOT 0
#endif

INTERFACE_END

#endif // BLIT_REDUCED_INTERFACE_GLSL