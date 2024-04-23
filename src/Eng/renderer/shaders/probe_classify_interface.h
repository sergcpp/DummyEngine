#ifndef PROBE_CLASSIFY_INTERFACE_H
#define PROBE_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeClassify)

struct Params {
    VEC4_TYPE grid_origin;
    IVEC4_TYPE grid_scroll;
    VEC4_TYPE grid_spacing;
};

const int LOCAL_GROUP_SIZE_X = 32;

const int RAY_DATA_TEX_SLOT = 1;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_CLASSIFY_INTERFACE_H