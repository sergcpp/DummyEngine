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

	const int DEPTH_TEX_SLOT = 3;
	const int NORM_TEX_SLOT = 4;
	const int PREV_TEX_SLOT = 5;
	const int ROUGH_TEX_SLOT = 6;
	const int RAY_COUNTER_SLOT = 7;
	const int IN_RAY_LIST_SLOT = 8;
	const int SOBOL_BUF_SLOT = 9;
	const int SCRAMLING_TILE_BUF_SLOT = 10;
	const int RANKING_TILE_BUF_SLOT = 11;
	
	const int OUT_COLOR_IMG_SLOT = 0;
	const int OUT_RAYLEN_IMG_SLOT = 1;
	const int OUT_RAY_LIST_SLOT = 2;
#else
	#define LOCAL_GROUP_SIZE_X 8
	#define LOCAL_GROUP_SIZE_Y 8

	#define DEPTH_TEX_SLOT 3
	#define NORM_TEX_SLOT 4
	#define PREV_TEX_SLOT 5
	#define ROUGH_TEX_SLOT 6
	#define RAY_COUNTER_SLOT 7
	#define IN_RAY_LIST_SLOT 8
	#define SOBOL_BUF_SLOT 9
	#define SCRAMLING_TILE_BUF_SLOT 10
	#define RANKING_TILE_BUF_SLOT 11
	
	#define OUT_COLOR_IMG_SLOT 0
	#define OUT_RAYLEN_IMG_SLOT 1
	#define OUT_RAY_LIST_SLOT 2
#endif

INTERFACE_END

#endif // SSR_TRACE_HQ_INTERFACE_GLSL