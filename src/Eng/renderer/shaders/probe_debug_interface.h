#ifndef PROBE_DEBUG_INTERFACE_H
#define PROBE_DEBUG_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeDebug)

struct Params {
    uint volume_index;
    uint _pad0;
    uint _pad1;
    uint _pad2;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
};

const uint IRRADIANCE_TEX_SLOT = 0;
const uint OFFSET_TEX_SLOT = 1;

INTERFACE_END

#endif // PROBE_DEBUG_INTERFACE_H