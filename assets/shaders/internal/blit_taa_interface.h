#ifndef BLIT_TAA_INTERFACE_H
#define BLIT_TAA_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(TempAA)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE tex_size;
    float tonemap;
    float gamma;
    float exposure;
    float fade;
    float mix_factor; // for static accumulation
};

DEF_CONST_INT(CURR_TEX_SLOT, 0)
DEF_CONST_INT(HIST_TEX_SLOT, 1);
DEF_CONST_INT(DEPTH_TEX_SLOT, 2);
DEF_CONST_INT(VELOCITY_TEX_SLOT, 3);

INTERFACE_END

#endif // BLIT_TAA_INTERFACE_H