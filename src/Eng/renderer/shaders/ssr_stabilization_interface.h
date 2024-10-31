#ifndef SSR_STABILIZATION_INTERFACE_H
#define SSR_STABILIZATION_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRStabilization)

struct Params {
    uvec2 img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int VELOCITY_TEX_SLOT = 2;
const int SSR_TEX_SLOT = 3;
const int SSR_HIST_TEX_SLOT = 4;
const int SAMPLE_COUNT_TEX_SLOT = 5;

const int OUT_SSR_IMG_SLOT = 0;

INTERFACE_END

#endif // SSR_STABILIZATION_INTERFACE_H