#ifndef PREPARE_DISOCCLUSION_INTERFACE_H
#define PREPARE_DISOCCLUSION_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(PrepareDisocclusion)

struct Params {
    uvec2 img_size;
    vec2 texel_size;
    vec4 clip_info;
    vec4 frustum_info;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DILATED_DEPTH_TEX_SLOT = 1;
const uint DILATED_VELOCITY_TEX_SLOT = 2;
const uint RECONSTRUCTED_DEPTH_TEX_SLOT = 3;
const uint VELOCITY_TEX_SLOT = 4;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PREPARE_DISOCCLUSION_INTERFACE_H