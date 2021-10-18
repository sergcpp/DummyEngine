#ifndef SSR_BLUR_INTERFACE_GLSL
#define SSR_BLUR_INTERFACE_GLSL

#include "_interface_common.glsl"

INTERFACE_START(SSRBlur)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE thresholds;
};

#ifdef __cplusplus
    const int LOCAL_GROUP_SIZE_X = 8;
    const int LOCAL_GROUP_SIZE_Y = 8;

    const int ROUGH_TEX_SLOT = 0;
    const int REFL_TEX_SLOT = 1;
    const int TILE_METADATA_MASK_SLOT = 2;
    const int OUT_DENOISED_IMG_SLOT = 3;

#else
    #define LOCAL_GROUP_SIZE_X 8
    #define LOCAL_GROUP_SIZE_Y 8

    #define ROUGH_TEX_SLOT 0
    #define REFL_TEX_SLOT 1
    #define TILE_METADATA_MASK_SLOT 2
    #define OUT_DENOISED_IMG_SLOT 3

#endif

INTERFACE_END

#endif // SSR_BLUR_INTERFACE_GLSL