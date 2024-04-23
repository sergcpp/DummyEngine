#ifndef PROBE_SAMPLE_INTERFACE_H
#define PROBE_SAMPLE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeSample)

struct Params {
    VEC4_TYPE grid_origin;
    IVEC4_TYPE grid_scroll;
    VEC4_TYPE grid_spacing;
    UVEC2_TYPE img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int NORMAL_TEX_SLOT = 2;
const int IRRADIANCE_TEX_SLOT = 3;
const int DISTANCE_TEX_SLOT = 4;
const int OFFSET_TEX_SLOT = 5;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_SAMPLE_INTERFACE_H