#ifndef PROBE_CLASSIFY_INTERFACE_H
#define PROBE_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeClassify)

struct Params {
    uint volume_index;
    uint oct_index;
    uint _pad1;
    uint _pad2;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
    vec4 quat_rot;
    vec4 vol_bbox_min;
    vec4 vol_bbox_max;
};

const uint GRP_SIZE_X = 32;

const uint RAY_DATA_TEX_SLOT = 1;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_CLASSIFY_INTERFACE_H