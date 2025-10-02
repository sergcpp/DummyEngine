#ifndef GI_STABILIZATION_INTERFACE_H
#define GI_STABILIZATION_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GIStabilization)

struct Params {
    uvec2 img_size;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int VELOCITY_TEX_SLOT = 2;
const int GI_TEX_SLOT = 3;
const int GI_HIST_TEX_SLOT = 4;
const int SAMPLE_COUNT_TEX_SLOT = 5;

const int OUT_GI_IMG_SLOT = 0;

INTERFACE_END

#endif // GI_STABILIZATION_INTERFACE_H