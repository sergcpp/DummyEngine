#ifndef MOTION_BLUR_INTERFACE_H
#define MOTION_BLUR_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(MotionBlur)

struct Params {
    uvec2 img_size;
    vec2 inv_ren_res;
    vec4 clip_info;
};

const int TILE_RES = 20;

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int GRP_SIZE = GRP_SIZE_X * GRP_SIZE_Y;

const int COLOR_TEX_SLOT = 1;
const int DEPTH_TEX_SLOT = 2;
const int VELOCITY_TEX_SLOT = 3;
const int TILES_TEX_SLOT = 4;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // MOTION_BLUR_CLASSIFY_INTERFACE_H