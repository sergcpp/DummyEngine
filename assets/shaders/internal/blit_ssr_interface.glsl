
#include "_interface_common.glsl"

INTERFACE_START(SSRTrace)

struct Params {
	VEC4_TYPE transform;
};

#ifdef __cplusplus
	const int DEPTH_TEX_SLOT = 0;
	const int NORM_TEX_SLOT = 1;
	const int SPEC_TEX_SLOT = 2;
#else
	#define DEPTH_TEX_SLOT 0
	#define NORM_TEX_SLOT 1
	#define SPEC_TEX_SLOT 2
#endif

INTERFACE_END