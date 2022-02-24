#ifndef SHADOW_INTERFACE_GLSL
#define SHADOW_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Shadow)

struct Params {
    MAT4_TYPE uShadowViewProjMatrix;
    IVEC2_TYPE uInstanceIndices[REN_MAX_BATCH_SIZE];
};

DEF_CONST_INT(U_M_MATRIX_LOC, 12)

INTERFACE_END

#endif // SHADOW_INTERFACE_GLSL