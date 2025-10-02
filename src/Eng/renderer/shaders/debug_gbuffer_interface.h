#ifndef DEBUG_GBUFFER_INTERFACE_H
#define DEBUG_GBUFFER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(DebugGBuffer)

struct Params {
    uvec2 img_size;
};

const int GRP_SIZE_X = 8;
const int GRP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int ALBEDO_TEX_SLOT = 2;
const int NORM_TEX_SLOT = 3;
const int SPEC_TEX_SLOT = 4;

const int OUT_IMG_SLOT = 0;

INTERFACE_END

#endif // DEBUG_GBUFFER_INTERFACE_H