#ifndef SKYDOME_DOWNSAMPLE_INTERFACE_H
#define SKYDOME_DOWNSAMPLE_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SkydomeDownsample)

struct Params {
    uvec2 img_size;
    int mip_count;
    int _pad0;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint INPUT_TEX_SLOT = 0;
const uint OUTPUT_IMG_SLOT = 1;

INTERFACE_END

#endif // SKYDOME_DOWNSAMPLE_INTERFACE_H