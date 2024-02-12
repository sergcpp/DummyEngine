#ifndef BLIT_COMBINE_INTERFACE_H
#define BLIT_COMBINE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(BlitCombine)

struct Params {
    VEC4_TYPE transform;
    VEC2_TYPE tex_size;
    float tonemap;
    float gamma;
    float exposure;
    float fade;
};

const int HDR_TEX_SLOT = 0;
const int BLURED_TEX_SLOT = 1;

INTERFACE_END

#endif // BLIT_COMBINE_INTERFACE_H