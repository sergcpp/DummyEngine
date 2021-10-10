#ifndef SSR_RESOLVE_TEMPORAL_INTERFACE_GLSL
#define SSR_RESOLVE_TEMPORAL_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRResolveTemporal)

struct Params {
	UVEC2_TYPE img_size;
	VEC2_TYPE thresholds;
};

#ifdef __cplusplus
	const int LOCAL_GROUP_SIZE_X = 8;
	const int LOCAL_GROUP_SIZE_Y = 8;

	const int DEPTH_TEX_SLOT = 11;
	const int NORM_TEX_SLOT = 1;
	const int ROUGH_TEX_SLOT = 2;
	const int NORM_HIST_TEX_SLOT = 3;
	const int ROUGH_HIST_TEX_SLOT = 4;
	const int VELOCITY_TEX_SLOT = 5;
	const int REFL_TEX_SLOT = 6;
	const int REFL_HIST_TEX_SLOT = 7;
	const int RAY_LEN_TEX_SLOT = 8;
	const int TILE_METADATA_MASK_SLOT = 9;
	const int TEMP_VARIANCE_MASK_SLOT = 10;
	const int OUT_DENOISED_IMG_SLOT = 0;

#else
	#define LOCAL_GROUP_SIZE_X 8
	#define LOCAL_GROUP_SIZE_Y 8

	#define DEPTH_TEX_SLOT 11
	#define NORM_TEX_SLOT 1
	#define ROUGH_TEX_SLOT 2
	#define NORM_HIST_TEX_SLOT 3
	#define ROUGH_HIST_TEX_SLOT 4
	#define VELOCITY_TEX_SLOT 5
	#define REFL_TEX_SLOT 6
	#define REFL_HIST_TEX_SLOT 7
	#define RAY_LEN_TEX_SLOT 8
	#define TILE_METADATA_MASK_SLOT 9
	#define TEMP_VARIANCE_MASK_SLOT 10
	#define OUT_DENOISED_IMG_SLOT 0

#endif

INTERFACE_END

#endif // SSR_RESOLVE_TEMPORAL_INTERFACE_GLSL