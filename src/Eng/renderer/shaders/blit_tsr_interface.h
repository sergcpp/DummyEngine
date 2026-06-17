#ifndef BLIT_TSR_INTERFACE_H
#define BLIT_TSR_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(TSR)

struct Params {
    vec4 transform;
    vec4 texel_size;
    vec2 unjitter;
    float significant_change;
    float tonemap;
    float gamma;
    float fade;
    float mix_factor;
    float downscale_factor;
};

const uint CURR_NEAREST_TEX_SLOT = 0;
const uint CURR_LINEAR_TEX_SLOT = 1;
const uint HIST_TEX_SLOT = 2;
const uint DILATED_DEPTH_TEX_SLOT = 4;
const uint DILATED_VELOCITY_TEX_SLOT = 5;
const uint DISOCCLUSION_MASK_TEX_SLOT = 6;

INTERFACE_END

#endif // BLIT_TSR_INTERFACE_H