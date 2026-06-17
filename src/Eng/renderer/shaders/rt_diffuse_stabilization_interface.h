#ifndef RT_DIFFUSE_STABILIZATION_INTERFACE_H
#define RT_DIFFUSE_STABILIZATION_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuseStabilization)

struct Params {
    uvec2 img_size;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint VELOCITY_TEX_SLOT = 2;
const uint GI_TEX_SLOT = 3;
const uint GI_HIST_TEX_SLOT = 4;
const uint SAMPLE_COUNT_TEX_SLOT = 5;

const uint OUT_GI_IMG_SLOT = 0;

INTERFACE_END

#endif // RT_DIFFUSE_STABILIZATION_INTERFACE_H