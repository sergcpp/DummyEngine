#ifndef BLIT_SSR_COMPOSE2_INTERFACE_GLSL
#define BLIT_SSR_COMPOSE2_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRCompose2)

struct Params {
	VEC4_TYPE transform;
};

#ifdef __cplusplus
	const int SPEC_TEX_SLOT = 0;
	const int DEPTH_TEX_SLOT = 1;
	const int NORM_TEX_SLOT = 2;
	const int REFL_TEX_SLOT = 3;
	const int BRDF_TEX_SLOT = 4;
#else
	#define SPEC_TEX_SLOT 0
	#define DEPTH_TEX_SLOT 1
	#define NORM_TEX_SLOT 2
	#define REFL_TEX_SLOT 3
	#define BRDF_TEX_SLOT 4
#endif

INTERFACE_END

#endif // BLIT_SSR_COMPOSE2_INTERFACE_GLSL