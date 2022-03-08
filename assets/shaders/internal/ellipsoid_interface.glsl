#ifndef ELLIPSOID_INTERFACE_GLSL
#define ELLIPSOID_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Ellipsoid)

/*struct Params {
    IVEC2_TYPE g_instance_indices[REN_MAX_BATCH_SIZE][2];
    MAT4_TYPE g_shadow_view_proj_mat;
};*/

DEF_CONST_INT(U_M_MATRIX_LOC, 0)

INTERFACE_END

#endif // ELLIPSOID_INTERFACE_GLSL