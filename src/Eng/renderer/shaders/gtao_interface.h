#ifndef GTAO_INTERFACE_H
#define GTAO_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GTAO)

struct Params {
    uvec2 img_size;
    vec2 rand;
    vec4 clip_info;
    vec4 frustum_info;
    mat4 view_from_world;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int NORM_TEX_SLOT = 2;
const int GTAO_TEX_SLOT = 3;
const int GTAO_HIST_TEX_SLOT = 4;
const int VELOCITY_TEX_SLOT = 5;
const int DEPTH_HIST_TEX_SLOT = 6;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // GTAO_INTERFACE_H