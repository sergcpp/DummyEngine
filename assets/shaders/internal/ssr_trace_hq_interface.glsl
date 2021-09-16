#ifndef SSR_TRACE_HQ_INTERFACE_GLSL
#define SSR_TRACE_HQ_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRTraceHQ)

struct Params {
	UVEC4_TYPE resolution;
};

#ifdef __cplusplus
	const int LOCAL_GROUP_SIZE_X = 8;
	const int LOCAL_GROUP_SIZE_Y = 8;

	const int DEPTH_TEX_SLOT = 0;
	const int NORM_TEX_SLOT = 1;
	const int OUTPUT_TEX_SLOT = 2;
#else
	#define LOCAL_GROUP_SIZE_X 8
	#define LOCAL_GROUP_SIZE_Y 8

	#define DEPTH_TEX_SLOT 0
	#define NORM_TEX_SLOT 1
	#define OUTPUT_TEX_SLOT 2
#endif

INTERFACE_END

#endif // SSR_TRACE_HQ_INTERFACE_GLSL