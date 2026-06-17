#ifndef RT_SPECULAR_TRACE_SS_INTERFACE_H
#define RT_SPECULAR_TRACE_SS_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTSpecularTraceSS)

struct Params {
    uvec4 resolution;
};

const uint GRP_SIZE_X = 64;

const uint DEPTH_TEX_SLOT = 4;
const uint COLOR_TEX_SLOT = 5;
const uint NORM_TEX_SLOT = 6;
const uint ALBEDO_TEX_SLOT = 7;
const uint SPEC_TEX_SLOT = 8;
const uint LTC_LUTS_TEX_SLOT = 9;
const uint IRRADIANCE_TEX_SLOT = 10;
const uint DISTANCE_TEX_SLOT = 11;
const uint OFFSET_TEX_SLOT = 12;
const uint IN_RAY_LIST_SLOT = 13;
const uint NOISE_TEX_SLOT = 14;
const uint OIT_DEPTH_BUF_SLOT = 14;

const uint OUT_REFL_IMG_SLOT = 0;
const uint INOUT_RAY_COUNTER_SLOT = 2;
const uint OUT_RAY_LIST_SLOT = 3;

INTERFACE_END

#endif // RT_SPECULAR_TRACE_SS_INTERFACE_H