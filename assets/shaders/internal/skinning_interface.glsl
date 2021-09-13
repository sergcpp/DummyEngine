#ifndef SKINNING_INTERFACE_GLSL
#define SKINNING_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Skinning)

struct Params {
	UVEC4_TYPE uSkinParams;
	UVEC4_TYPE uShapeParamsCurr;
	UVEC4_TYPE uShapeParamsPrev;
};

#ifdef __cplusplus
	const int LOCAL_GROUP_SIZE = 128;

	const int IN_VERTICES_SLOT = 0;
	const int IN_MATRICES_SLOT = 1;
	const int IN_SHAPE_KEYS_SLOT = 2;
	const int IN_DELTAS_SLOT = 3;
	const int OUT_VERTICES0 = 4;
	const int OUT_VERTICES1 = 5;
#else
	#define LOCAL_GROUP_SIZE 128

	#define IN_VERTICES_SLOT 0
	#define IN_MATRICES_SLOT 1
	#define IN_SHAPE_KEYS_SLOT 2
	#define IN_DELTAS_SLOT 3
	#define OUT_VERTICES0 4
	#define OUT_VERTICES1 5
#endif

INTERFACE_END

#endif // SKINNING_INTERFACE_GLSL