#ifndef RT_REFLECTIONS_INTERFACE_GLSL
#define RT_REFLECTIONS_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(RTReflections)

struct Params {
    IVEC4_TYPE depth_size;
    VEC4_TYPE clip_info;
};

struct RayPayload {
    VEC4_TYPE col;
};

#ifdef __cplusplus
    const int TLAS_SLOT = 0;
	const int DEPTH_TEX_SLOT = 1;
	const int NORM_TEX_SLOT = 2;
	const int SPEC_TEX_SLOT = 3;
	const int SSR_TEX_SLOT = 4;
	const int PREV_TEX_SLOT = 5;
	const int BRDF_TEX_SLOT = 6;
    const int ENV_TEX_SLOT = 7;
    const int GEO_DATA_BUF_SLOT = 8;
    const int MATERIAL_BUF_SLOT = 9;
    const int VTX_BUF1_SLOT = 10;
    const int VTX_BUF2_SLOT = 11;
    const int NDX_BUF_SLOT = 12;
    const int LMAP_TEX_SLOTS = 13;
    const int OUT_IMG_SLOT = 18;
#else
    #define TLAS_SLOT 0
	#define DEPTH_TEX_SLOT 1
	#define NORM_TEX_SLOT 2
	#define SPEC_TEX_SLOT 3
	#define SSR_TEX_SLOT 4
	#define PREV_TEX_SLOT 5
	#define BRDF_TEX_SLOT 6
    #define ENV_TEX_SLOT 7
    #define GEO_DATA_BUF_SLOT 8
    #define MATERIAL_BUF_SLOT 9
    #define VTX_BUF1_SLOT 10
    #define VTX_BUF2_SLOT 11
    #define NDX_BUF_SLOT 12
    #define LMAP_TEX_SLOTS 13
    #define OUT_IMG_SLOT 18
#endif

INTERFACE_END

#endif // RT_REFLECTIONS_INTERFACE_GLSL