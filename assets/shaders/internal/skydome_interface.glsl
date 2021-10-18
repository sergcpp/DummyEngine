#ifndef SKYDOME_INTERFACE_GLSL
#define SKYDOME_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Skydome)

/*struct Params {
    IVEC2_TYPE uInstanceIndices[REN_MAX_BATCH_SIZE][2];
    MAT4_TYPE uShadowViewProjMatrix;
};*/

#ifdef __cplusplus
    const int U_M_MATRIX_LOC = 0;
    const int ENV_TEX_SLOT = 1;
#else
    #define U_M_MATRIX_LOC 0
    #define ENV_TEX_SLOT 1
#endif

INTERFACE_END

#endif // SKYDOME_INTERFACE_GLSL