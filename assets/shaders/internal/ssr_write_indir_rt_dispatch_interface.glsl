#ifndef SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_GLSL
#define SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRWriteIndirRTDispatch)

#ifdef __cplusplus
	const int RAY_COUNTER_SLOT = 0;
	const int INDIR_ARGS_SLOT = 1;
#else
	#define RAY_COUNTER_SLOT 0
	#define INDIR_ARGS_SLOT 1
#endif

INTERFACE_END

#endif // SSR_WRITE_INDIR_RT_DISPATCH_INTERFACE_GLSL