#ifndef BLIT_UPSCALE_INTERFACE_GLSL
#define BLIT_UPSCALE_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(Upscale)

struct Params {
	VEC4_TYPE transform;
	VEC4_TYPE resolution;
	VEC4_TYPE clip_info;
};

#ifdef __cplusplus
	const int DEPTH_TEX_SLOT = 0;
	const int DEPTH_LOW_TEX_SLOT = 1;
	const int INPUT_TEX_SLOT = 2;
#else
	#define DEPTH_TEX_SLOT 0
	#define DEPTH_LOW_TEX_SLOT 1
	#define INPUT_TEX_SLOT 2
#endif

INTERFACE_END

#endif // BLIT_UPSCALE_INTERFACE_GLSL