#ifndef PROBE_BLEND_INTERFACE_H
#define PROBE_BLEND_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeBlend)

struct Params {
    int volume_index;
    float hysteresis;
    int input_offset;
    int output_offset;
    vec4 grid_origin;
    ivec4 grid_scroll;
    ivec4 grid_scroll_diff;
    vec4 grid_spacing;
    vec4 quat_rot;
};

const int RAY_DATA_TEX_SLOT = 1;
const int OFFSET_TEX_SLOT = 2;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_BLEND_INTERFACE_H