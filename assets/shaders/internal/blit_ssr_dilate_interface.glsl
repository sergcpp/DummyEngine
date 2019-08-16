
#include "_interface_common.glsl"

INTERFACE_START(SSRDilate)

struct Params {
	VEC4_TYPE transform;
};

#ifdef __cplusplus
	const int SSR_TEX_SLOT = 0;
#else
	#define SSR_TEX_SLOT 0
#endif

INTERFACE_END