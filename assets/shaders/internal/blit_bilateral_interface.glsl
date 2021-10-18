#ifndef BLIT_BILATERAL_INTERFACE_GLSL
#define BLIT_BILATERAL_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Bilateral)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE resolution;
    VEC2_TYPE _pad;
    float vertical;
};

#ifdef __cplusplus
    const int DEPTH_TEX_SLOT = 0;
    const int INPUT_TEX_SLOT = 1;
#else
    #define DEPTH_TEX_SLOT 0
    #define INPUT_TEX_SLOT 1
#endif

INTERFACE_END

#endif // BLIT_BILATERAL_INTERFACE_GLSL