#ifndef PROBE_RELOCATE_INTERFACE_H
#define PROBE_RELOCATE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeRelocate)

struct Params {
    vec4 grid_origin;
    ivec4 grid_scroll;
    vec4 grid_spacing;
};

const int LOCAL_GROUP_SIZE_X = 32;

const int RAY_DATA_TEX_SLOT = 1;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_RELOCATE_INTERFACE_H