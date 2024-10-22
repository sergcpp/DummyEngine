#ifndef PROBE_DEBUG_INTERFACE_H
#define PROBE_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeDebug)

struct Params {
    int volume_index;
    int _pad0;
    int _pad1;
    int _pad2;
    vec4 grid_origin;
    ivec4 grid_scroll;
    vec4 grid_spacing;
};

const int IRRADIANCE_TEX_SLOT = 0;
const int OFFSET_TEX_SLOT = 1;

INTERFACE_END

#endif // PROBE_DEBUG_INTERFACE_H