#ifndef HISTOGRAM_EXPOSURE_INTERFACE_H
#define HISTOGRAM_EXPOSURE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(HistogramExposure)

struct Params {
    float min_exposure;
    float max_exposure;
    float exposure_factor;
    float adaptation_speed_max;
    float adaptation_speed_min;
    float _pad1;
    float _pad2;
};

const uint HISTOGRAM_TEX_SLOT = 1;
const uint EXPOSURE_PREV_TEX_SLOT = 2;
const uint OUT_TEX_SLOT = 0;

INTERFACE_END

#endif // HISTOGRAM_EXPOSURE_INTERFACE_H