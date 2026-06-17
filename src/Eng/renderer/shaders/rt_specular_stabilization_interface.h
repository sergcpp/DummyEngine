#ifndef RT_SPECULAR_STABILIZATION_INTERFACE_H
#define RT_SPECULAR_STABILIZATION_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTSpecularStabilization)

struct Params {
    uvec2 img_size;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint VELOCITY_TEX_SLOT = 2;
const uint SSR_TEX_SLOT = 3;
const uint SSR_HIST_TEX_SLOT = 4;
const uint SAMPLE_COUNT_TEX_SLOT = 5;

const uint OUT_SSR_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_SPECULAR_STABILIZATION_INTERFACE_H