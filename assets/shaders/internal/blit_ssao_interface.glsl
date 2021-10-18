#ifndef BLIT_SSAO_INTERFACE_GLSL
#define BLIT_SSAO_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSAO)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE resolution;
};

#ifdef __cplusplus
    const int DEPTH_TEX_SLOT = 0;
    const int RAND_TEX_SLOT = 1;
#else
    #define DEPTH_TEX_SLOT 0
    #define RAND_TEX_SLOT 1
#endif

INTERFACE_END

#endif // BLIT_SSAO_INTERFACE_GLSL