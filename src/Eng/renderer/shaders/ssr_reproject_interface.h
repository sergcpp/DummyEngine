#ifndef SSR_REPROJECT_INTERFACE_H
#define SSR_REPROJECT_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(SSRReproject)

struct Params {
    uvec2 img_size;
    vec2 thresholds;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int DEPTH_TEX_SLOT = 4;
const int NORM_TEX_SLOT = 5;
const int DEPTH_HIST_TEX_SLOT = 6;
const int NORM_HIST_TEX_SLOT = 7;
const int REFL_TEX_SLOT = 8;
const int REFL_HIST_TEX_SLOT = 9;
const int VELOCITY_TEX_SLOT = 10;
const int VARIANCE_HIST_TEX_SLOT = 11;
const int SAMPLE_COUNT_HIST_TEX_SLOT = 12;
const int TILE_LIST_BUF_SLOT = 13;

const int OUT_REPROJECTED_IMG_SLOT = 0;
const int OUT_AVG_REFL_IMG_SLOT = 1;
const int OUT_VERIANCE_IMG_SLOT = 2;
const int OUT_SAMPLE_COUNT_IMG_SLOT = 3;

INTERFACE_END

#endif // SSR_REPROJECT_INTERFACE_H