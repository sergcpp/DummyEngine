#ifndef PROBE_DEBUG_INTERFACE_H
#define PROBE_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeDebug)

struct Params {
    VEC4_TYPE grid_origin;
    IVEC4_TYPE grid_scroll;
    VEC4_TYPE grid_spacing;
};

const int IRRADIANCE_TEX_SLOT = 0;
const int OFFSET_TEX_SLOT = 1;

INTERFACE_END

#endif // PROBE_DEBUG_INTERFACE_H