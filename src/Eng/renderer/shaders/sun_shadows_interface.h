#ifndef SUN_SHADOWS_INTERFACE_H
#define SUN_SHADOWS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SunShadows)

struct Params {
    UVEC2_TYPE img_size;
    VEC2_TYPE enabled;
    VEC4_TYPE softness_factor;
};

struct RayPayload {
    VEC3_TYPE col;
    float cone_width;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int NORM_TEX_SLOT = 2;
const int SHADOW_TEX_SLOT = 3;
const int SHADOW_TEX_VAL_SLOT = 4;
const int OUT_SHADOW_IMG_SLOT = 0;

INTERFACE_END

#endif // SUN_SHADOWS_INTERFACE_H