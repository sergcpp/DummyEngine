#ifndef BLIT_COMBINE_INTERFACE_GLSL
#define BLIT_COMBINE_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(BlitCombine)

struct Params {
	VEC4_TYPE transform;
	VEC2_TYPE tex_size;
	float tonemap;
	float gamma;
	float exposure;
	float fade;
};

#ifdef __cplusplus
	const int HDR_TEX_SLOT = 0;
	const int BLURED_TEX_SLOT = 1;
#else
	#define HDR_TEX_SLOT 0
	#define BLURED_TEX_SLOT 1
#endif

INTERFACE_END

#endif // BLIT_COMBINE_INTERFACE_GLSL