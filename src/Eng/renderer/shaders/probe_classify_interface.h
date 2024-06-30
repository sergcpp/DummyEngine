#ifndef PROBE_CLASSIFY_INTERFACE_H
#define PROBE_CLASSIFY_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeClassify)

struct Params {
    int volume_index;
    int _pad0;
    int _pad1;
    int _pad2;
    vec4 grid_origin;
    ivec4 grid_scroll;
    vec4 grid_spacing;
    vec4 quat_rot;
};

const int LOCAL_GROUP_SIZE_X = 32;

const int RAY_DATA_TEX_SLOT = 1;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_CLASSIFY_INTERFACE_H