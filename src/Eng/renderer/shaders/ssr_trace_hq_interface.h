#ifndef SSR_TRACE_HQ_INTERFACE_H
#define SSR_TRACE_HQ_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRTraceHQ)

struct Params {
    uvec4 resolution;
};

const int GRP_SIZE_X = 64;

const int DEPTH_TEX_SLOT = 4;
const int COLOR_TEX_SLOT = 5;
const int NORM_TEX_SLOT = 6;
const int ALBEDO_TEX_SLOT = 7;
const int SPEC_TEX_SLOT = 8;
const int LTC_LUTS_TEX_SLOT = 9;
const int IRRADIANCE_TEX_SLOT = 10;
const int DISTANCE_TEX_SLOT = 11;
const int OFFSET_TEX_SLOT = 12;
const int IN_RAY_LIST_SLOT = 13;
const int NOISE_TEX_SLOT = 14;
const int OIT_DEPTH_BUF_SLOT = 14;

const int OUT_REFL_IMG_SLOT = 0;
const int INOUT_RAY_COUNTER_SLOT = 2;
const int OUT_RAY_LIST_SLOT = 3;

INTERFACE_END

#endif // SSR_TRACE_HQ_INTERFACE_H