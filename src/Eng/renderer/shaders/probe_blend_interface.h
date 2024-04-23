#ifndef PROBE_BLEND_INTERFACE_H
#define PROBE_BLEND_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeBlend)

struct Params {
    VEC4_TYPE grid_origin;
    IVEC4_TYPE grid_scroll;
    VEC4_TYPE grid_spacing;
};

const int RAY_DATA_TEX_SLOT = 1;
const int OFFSET_TEX_SLOT = 2;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_BLEND_INTERFACE_H