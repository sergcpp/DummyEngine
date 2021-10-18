#ifndef RT_REFLECTIONS_INTERFACE_GLSL
#define RT_REFLECTIONS_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(RTReflections)

struct Params {
	UVEC2_TYPE img_size;
    float pixel_spread_angle;
    float _pad[1];
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

#ifdef __cplusplus
    const int TLAS_SLOT = 0;
    const int DEPTH_TEX_SLOT = 1;
    const int NORM_TEX_SLOT = 2;
    const int SPEC_TEX_SLOT = 3;
    const int ROUGH_TEX_SLOT = 4;
    const int ENV_TEX_SLOT = 5;
    const int GEO_DATA_BUF_SLOT = 6;
    const int MATERIAL_BUF_SLOT = 7;
    const int VTX_BUF1_SLOT = 8;
    const int VTX_BUF2_SLOT = 9;
    const int NDX_BUF_SLOT = 10;
	const int RAY_LIST_SLOT = 11;
	const int SOBOL_BUF_SLOT = 12;
	const int SCRAMLING_TILE_BUF_SLOT = 13;
	const int RANKING_TILE_BUF_SLOT = 14;
    const int LMAP_TEX_SLOTS = 15;
    const int OUT_COLOR_IMG_SLOT = 20;
	const int OUT_RAYLEN_IMG_SLOT = 22;
#else
    #define TLAS_SLOT 0
    #define DEPTH_TEX_SLOT 1
    #define NORM_TEX_SLOT 2
    #define SPEC_TEX_SLOT 3
    #define ROUGH_TEX_SLOT 4
    #define ENV_TEX_SLOT 5
    #define GEO_DATA_BUF_SLOT 6
    #define MATERIAL_BUF_SLOT 7
    #define VTX_BUF1_SLOT 8
    #define VTX_BUF2_SLOT 9
    #define NDX_BUF_SLOT 10
	#define RAY_LIST_SLOT 11
	#define SOBOL_BUF_SLOT 12
	#define SCRAMLING_TILE_BUF_SLOT 13
	#define RANKING_TILE_BUF_SLOT 14
    #define LMAP_TEX_SLOTS 15
    #define OUT_COLOR_IMG_SLOT 20
	#define OUT_RAYLEN_IMG_SLOT 22
#endif

INTERFACE_END

#endif // RT_REFLECTIONS_INTERFACE_GLSL