#ifndef RECONSTRUCT_DEPTH_INTERFACE_H
#define RECONSTRUCT_DEPTH_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ReconstructDepth)

struct Params {
    uvec2 img_size;
    vec2 texel_size;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 3;
const uint VELOCITY_TEX_SLOT = 4;

const uint OUT_RECONSTRUCTED_DEPTH_IMG_SLOT = 0;
const uint OUT_DILATED_DEPTH_IMG_SLOT = 1;
const uint OUT_DILATED_VELOCITY_IMG_SLOT = 2;

INTERFACE_END

#endif // RECONSTRUCT_DEPTH_INTERFACE_H