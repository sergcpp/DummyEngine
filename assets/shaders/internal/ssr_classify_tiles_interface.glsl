#ifndef SSR_CLASSIFY_TILES_INTERFACE_GLSL
#define SSR_CLASSIFY_TILES_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRClassifyTiles)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE thresholds;
    UVEC2_TYPE samples_and_guided;
};

#ifdef __cplusplus
    const int LOCAL_GROUP_SIZE_X = 8;
    const int LOCAL_GROUP_SIZE_Y = 8;

    const int SPEC_TEX_SLOT = 0;
    const int TEMP_VARIANCE_MASK_SLOT = 1;
    const int RAY_COUNTER_SLOT = 2;
    const int RAY_LIST_SLOT = 3;
    const int TILE_METADATA_MASK_SLOT = 4;
    
    const int ROUGH_IMG_SLOT = 5;
#else
    #define LOCAL_GROUP_SIZE_X 8
    #define LOCAL_GROUP_SIZE_Y 8

    #define SPEC_TEX_SLOT 0
    #define TEMP_VARIANCE_MASK_SLOT 1
    #define RAY_COUNTER_SLOT 2
    #define RAY_LIST_SLOT 3
    #define TILE_METADATA_MASK_SLOT 4
    
    #define ROUGH_IMG_SLOT 5
#endif

INTERFACE_END

#endif // SSR_CLASSIFY_TILES_INTERFACE_GLSL