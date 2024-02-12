#ifndef SSR_TRACE_HQ_INTERFACE_H
#define SSR_TRACE_HQ_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRTraceHQ)

struct Params {
    UVEC4_TYPE resolution;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 4;
const int COLOR_TEX_SLOT = 5;
const int NORM_TEX_SLOT = 6;
const int IN_RAY_LIST_SLOT = 7;
const int NOISE_TEX_SLOT = 8;

const int OUT_REFL_IMG_SLOT = 0;
const int OUT_RAYLEN_IMG_SLOT = 1;
const int INOUT_RAY_COUNTER_SLOT = 2;
const int OUT_RAY_LIST_SLOT = 3;

INTERFACE_END

#endif // SSR_TRACE_HQ_INTERFACE_H