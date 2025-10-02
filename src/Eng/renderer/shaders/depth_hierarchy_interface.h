#ifndef DEPTH_HIERARCHY_INTERFACE_H
#define DEPTH_HIERARCHY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DepthHierarchy)

struct Params {
    ivec4 depth_size;
    vec4 clip_info;
};

const int GRP_SIZE_X = 64;
const int GRP_SIZE_Y = 64;

const int DEPTH_TEX_SLOT = 14;
const int ATOMIC_CNT_SLOT = 15;

const int DEPTH_IMG_SLOT = 0;

INTERFACE_END

#endif // DEPTH_HIERARCHY_INTERFACE_H