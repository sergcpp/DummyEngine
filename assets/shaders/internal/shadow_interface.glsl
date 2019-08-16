
#include "_interface_common.glsl"

INTERFACE_START(Shadow)

struct Params {
	MAT4_TYPE uShadowViewProjMatrix;
	IVEC2_TYPE uInstanceIndices[REN_MAX_BATCH_SIZE];
};

#ifdef __cplusplus
	const int U_M_MATRIX_LOC = 12;
#else
	#define U_M_MATRIX_LOC 12
#endif

INTERFACE_END