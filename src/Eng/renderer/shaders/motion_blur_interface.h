#ifndef MOTION_BLUR_INTERFACE_H
#define MOTION_BLUR_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(MotionBlur)

struct Params {
    uvec2 img_size;
    vec2 inv_ren_res;
    vec4 clip_info;
};

const uint TILE_RES = 20;

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint GRP_SIZE = GRP_SIZE_X * GRP_SIZE_Y;

const uint COLOR_TEX_SLOT = 1;
const uint DEPTH_TEX_SLOT = 2;
const uint VELOCITY_TEX_SLOT = 3;
const uint TILES_TEX_SLOT = 4;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // MOTION_BLUR_CLASSIFY_INTERFACE_H