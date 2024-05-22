#ifndef BLIT_TAA_INTERFACE_H
#define BLIT_TAA_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(TempAA)

struct Params {
    vec4 transform;
    vec2 tex_size;
    float tonemap;
    float gamma;
    float fade;
    float mix_factor; // for static accumulation
};

const int CURR_TEX_SLOT = 0;
const int HIST_TEX_SLOT = 1;
const int DEPTH_TEX_SLOT = 2;
const int VELOCITY_TEX_SLOT = 3;

INTERFACE_END

#endif // BLIT_TAA_INTERFACE_H