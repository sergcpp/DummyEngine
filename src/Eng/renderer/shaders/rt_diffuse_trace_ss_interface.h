#ifndef RT_DIFFUSE_TRACE_SS_INTERFACE_H
#define RT_DIFFUSE_TRACE_SS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuseTraceSS)

struct Params {
    uvec4 resolution;
};

const uint GRP_SIZE_X = 64;

const uint DEPTH_TEX_SLOT = 4;
const uint COLOR_TEX_SLOT = 5;
const uint NORM_TEX_SLOT = 6;
const uint IN_RAY_LIST_SLOT = 7;
const uint NOISE_TEX_SLOT = 8;

const uint OUT_GI_IMG_SLOT = 0;
const uint INOUT_RAY_COUNTER_SLOT = 2;
const uint OUT_RAY_LIST_SLOT = 3;

INTERFACE_END

#endif // RT_DIFFUSE_TRACE_SS_INTERFACE_H