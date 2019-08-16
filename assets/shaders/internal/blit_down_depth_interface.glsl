
#include "_interface_common.glsl"

INTERFACE_START(DownDepth)

struct Params {
	VEC4_TYPE transform;
	VEC4_TYPE clip_info;
	float linearize;
};

#ifdef __cplusplus
	const int DEPTH_TEX_SLOT = 0;
#else
	#define DEPTH_TEX_SLOT 0
#endif

INTERFACE_END