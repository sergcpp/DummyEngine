#ifndef PROBE_INTERFACE_GLSL
#define PROBE_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Probe)

/*struct Params {
	IVEC2_TYPE uInstanceIndices[REN_MAX_BATCH_SIZE][2];
	MAT4_TYPE uShadowViewProjMatrix;
};*/

#ifdef __cplusplus
	const int U_M_MATRIX_LOC = 0;
#else
	#define U_M_MATRIX_LOC 0
#endif

INTERFACE_END

#endif // PROBE_INTERFACE_GLSL