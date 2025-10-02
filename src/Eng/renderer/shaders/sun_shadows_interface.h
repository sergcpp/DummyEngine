#ifndef SUN_SHADOWS_INTERFACE_H
#define SUN_SHADOWS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SunShadows)

struct Params {
    uvec2 img_size;
    float _unused;
    float pixel_spread_angle;
    vec4 softness_factor;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int DEPTH_LIN_TEX_SLOT = 2;
const int ALBEDO_TEX_SLOT = 3;
const int NORM_TEX_SLOT = 4;
const int SHADOW_DEPTH_TEX_SLOT = 5;
const int SHADOW_DEPTH_TEX_VAL_SLOT = 6;
const int SHADOW_COLOR_TEX_SLOT = 7;
const int RT_SHADOW_TEX_SLOT = 8;

const int OUT_SHADOW_IMG_SLOT = 0;

INTERFACE_END

#endif // SUN_SHADOWS_INTERFACE_H