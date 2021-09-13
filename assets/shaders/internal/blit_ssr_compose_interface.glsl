#ifndef BLIT_SSR_COMPOSE_INTERFACE_GLSL
#define BLIT_SSR_COMPOSE_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRCompose)

struct Params {
	VEC4_TYPE transform;
};

#ifdef __cplusplus
	const int SSR_TEX_SLOT = 0;
#else
	#define SSR_TEX_SLOT 0
#endif

INTERFACE_END

#endif // BLIT_SSR_COMPOSE_INTERFACE_GLSL