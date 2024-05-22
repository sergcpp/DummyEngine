#ifndef HISTOGRAM_EXPOSURE_INTERFACE_H
#define HISTOGRAM_EXPOSURE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(HistogramExposure)

struct Params {
    float min_exposure;
    float max_exposure;
    float _pad0;
    float _pad1;
};

const int HISTOGRAM_TEX_SLOT = 1;
const int EXPOSURE_PREV_TEX_SLOT = 2;
const int OUT_TEX_SLOT = 0;

INTERFACE_END

#endif // HISTOGRAM_EXPOSURE_INTERFACE_H