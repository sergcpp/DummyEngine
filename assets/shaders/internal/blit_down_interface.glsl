
#include "_interface_common.glsl"

INTERFACE_START(DownColor)

struct Params {
	VEC4_TYPE transform;
	VEC4_TYPE resolution;
};

#ifdef __cplusplus
	const int SRC_TEX_SLOT = 0;
#else
	#define SRC_TEX_SLOT 0
#endif

INTERFACE_END