#ifndef HISTOGRAM_SAMPLE_INTERFACE_H
#define HISTOGRAM_SAMPLE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(HistogramSample)

struct Params {
    float scale;
    float _pad0;
    float _pad1;
    float _pad2;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int HDR_TEX_SLOT = 1;
const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // HISTOGRAM_SAMPLE_INTERFACE_H