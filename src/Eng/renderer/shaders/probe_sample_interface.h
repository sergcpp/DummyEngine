#ifndef PROBE_SAMPLE_INTERFACE_H
#define PROBE_SAMPLE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(ProbeSample)

struct Params {
    uvec2 img_size;
    uvec2 _pad;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 1;
const uint NORM_TEX_SLOT = 2;
const uint SSAO_TEX_SLOT = 3;
const uint IRRADIANCE_TEX_SLOT = 4;
const uint DISTANCE_TEX_SLOT = 5;
const uint OFFSET_TEX_SLOT = 6;

const uint OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // PROBE_SAMPLE_INTERFACE_H