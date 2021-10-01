#ifndef RT_DEBUG_INTERFACE_GLSL
#define RT_DEBUG_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(RTDebug)

struct Params {
    IVEC4_TYPE depth_size;
    VEC4_TYPE clip_info;
};

struct RayPayload {
    VEC4_TYPE col;
};

#ifdef __cplusplus
    const int TLAS_SLOT = 0;
    const int ENV_TEX_SLOT = 1;
    const int GEO_DATA_BUF_SLOT = 2;
    const int MATERIAL_BUF_SLOT = 3;
    const int VTX_BUF1_SLOT = 4;
    const int VTX_BUF2_SLOT = 5;
    const int NDX_BUF_SLOT = 6;
    const int LMAP_TEX_SLOTS = 7;
    const int OUT_IMG_SLOT = 12;
#else
    #define TLAS_SLOT 0
    #define ENV_TEX_SLOT 1
    #define GEO_DATA_BUF_SLOT 2
    #define MATERIAL_BUF_SLOT 3
    #define VTX_BUF1_SLOT 4
    #define VTX_BUF2_SLOT 5
    #define NDX_BUF_SLOT 6
    #define LMAP_TEX_SLOTS 7
    #define OUT_IMG_SLOT 12
#endif

INTERFACE_END

#endif // RT_DEBUG_INTERFACE_GLSL