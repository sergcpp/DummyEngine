#ifndef BLOOM_INTERFACE_H
#define BLOOM_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(Bloom)

struct Params {
    uvec2 img_size;
    float blend_weight;
    float pre_exposure;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int INPUT_TEX_SLOT = 1;
const int EXPOSURE_TEX_SLOT = 2;
const int BLEND_TEX_SLOT = 2;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // BLOOM_INTERFACE_H