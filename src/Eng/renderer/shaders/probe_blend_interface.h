#ifndef PROBE_BLEND_INTERFACE_H
#define PROBE_BLEND_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeBlend)

struct Params {
    uint volume_index;
    uint oct_index;
    float pre_exposure;
    float _pad0;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
    vec4 quat_rot;
};

const uint RAY_DATA_TEX_SLOT = 1;
const uint OFFSET_TEX_SLOT = 2;
const uint DIRECT_LIGHT_BUF_SLOT = 3;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_BLEND_INTERFACE_H