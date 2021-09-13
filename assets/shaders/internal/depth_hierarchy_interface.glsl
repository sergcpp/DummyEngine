#ifndef DEPTH_HIERARCHY_INTERFACE_GLSL
#define DEPTH_HIERARCHY_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(DepthHierarchy)

struct Params {
	IVEC4_TYPE depth_size;
	VEC4_TYPE clip_info;
};

#ifdef __cplusplus
	const int LOCAL_GROUP_SIZE_X = 64;
	const int LOCAL_GROUP_SIZE_Y = 64;

	const int DEPTH_TEX_SLOT = 0;
	const int DEPTH_IMG_SLOT = 1;
#else
	#define LOCAL_GROUP_SIZE_X 64
	#define LOCAL_GROUP_SIZE_Y 64

	#define DEPTH_TEX_SLOT 0
	#define DEPTH_IMG_SLOT 1
#endif

INTERFACE_END

#endif // DEPTH_HIERARCHY_INTERFACE_GLSL