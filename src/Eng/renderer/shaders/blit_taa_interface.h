#ifndef BLIT_TAA_INTERFACE_H
#define BLIT_TAA_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(TempAA)

struct Params {
    vec4 transform;
    vec2 texel_size;
    float significant_change;
    float frame_index;
    float tonemap;
    float gamma;
    float fade;
    float mix_factor; // for static accumulation
};

const int CURR_NEAREST_TEX_SLOT = 0;
const int CURR_LINEAR_TEX_SLOT = 1;
const int HIST_TEX_SLOT = 2;
const int DILATED_DEPTH_TEX_SLOT = 4;
const int DILATED_VELOCITY_TEX_SLOT = 5;
const int DISOCCLUSION_MASK_TEX_SLOT = 6;

INTERFACE_END

#endif // BLIT_TAA_INTERFACE_H