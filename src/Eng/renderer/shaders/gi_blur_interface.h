#ifndef GI_BLUR_INTERFACE_H
#define GI_BLUR_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GIBlur)

struct Params {
    VEC4_TYPE rotator;
    UVEC2_TYPE img_size;
    UVEC2_TYPE frame_index;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 1;
const int NORM_TEX_SLOT = 2;
const int GI_TEX_SLOT = 3;
const int SAMPLE_COUNT_TEX_SLOT = 4;
const int VARIANCE_TEX_SLOT = 5;
const int TILE_LIST_BUF_SLOT = 6;

const int OUT_DENOISED_IMG_SLOT = 0;

INTERFACE_END

#endif // GI_BLUR_INTERFACE_H