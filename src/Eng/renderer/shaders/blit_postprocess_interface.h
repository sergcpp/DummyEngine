#ifndef BLIT_POSTPROCESS_INTERFACE_H
#define BLIT_POSTPROCESS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(BlitPostprocess)

struct Params {
    vec4 transform;
    //
    vec2 tex_size;
    float tonemap_mode;
    float inv_gamma;
    //
    float aberration;
    float purkinje;
    float fade;
    float pre_exposure;
};

const int HDR_TEX_SLOT = 0;
const int BLOOM_TEX_SLOT = 1;
const int EXPOSURE_TEX_SLOT = 2;
const int LUT_TEX_SLOT = 3;

INTERFACE_END

#endif // BLIT_POSTPROCESS_INTERFACE_H