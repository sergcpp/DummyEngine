#ifndef PROBE_SAMPLE_INTERFACE_H
#define PROBE_SAMPLE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeSample)

struct Params {
    vec4 grid_origin;
    ivec4 grid_scroll;
    vec4 grid_spacing;
    uvec2 img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int NORMAL_TEX_SLOT = 2;
const int SSAO_TEX_SLOT = 3;
const int IRRADIANCE_TEX_SLOT = 4;
const int DISTANCE_TEX_SLOT = 5;
const int OFFSET_TEX_SLOT = 6;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_SAMPLE_INTERFACE_H