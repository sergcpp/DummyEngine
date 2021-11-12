#ifndef ELLIPSOID_INTERFACE_GLSL
#define ELLIPSOID_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Ellipsoid)

/*struct Params {
    IVEC2_TYPE uInstanceIndices[REN_MAX_BATCH_SIZE][2];
    MAT4_TYPE uShadowViewProjMatrix;
};*/

DEF_CONST_INT(U_M_MATRIX_LOC, 0)

INTERFACE_END

#endif // ELLIPSOID_INTERFACE_GLSL