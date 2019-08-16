
#include "_interface_common.glsl"

INTERFACE_START(BlitStaticVel)

struct Params {
	VEC4_TYPE transform;
};

#ifdef __cplusplus
	const int DEPTH_TEX_SLOT = 0;
#else
	#define DEPTH_TEX_SLOT 0
#endif

INTERFACE_END