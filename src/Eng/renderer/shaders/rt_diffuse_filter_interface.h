#ifndef RT_DIFFUSE_FILTER_INTERFACE_H
#define RT_DIFFUSE_FILTER_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuseFilter)

struct Params {
    vec4 rotator;
    uvec2 img_size;
    uvec2 frame_index;
    uint tile_count;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 3;
const uint SPEC_TEX_SLOT = 4;
const uint NORM_TEX_SLOT = 5;
const uint GI_TEX_SLOT = 6;
const uint AVG_GI_TEX_SLOT = 7;
const uint SAMPLE_COUNT_TEX_SLOT = 8;
const uint TILE_LIST_BUF_SLOT = 9;

const uint OUT_DENOISED_IMG_SLOT = 0;
const uint OUT_AVG_REFL_IMG_SLOT = 1;
const uint OUT_VARIANCE_IMG_SLOT = 2;

INTERFACE_END

#endif // RT_DIFFUSE_FILTER_INTERFACE_H