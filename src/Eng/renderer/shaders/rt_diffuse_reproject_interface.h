#ifndef RT_DIFFUSE_REPROJECT_INTERFACE_H
#define RT_DIFFUSE_REPROJECT_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTDiffuseReproject)

struct Params {
    uvec2 img_size;
    float hist_weight;
    float _unused0;
    vec2 unjitter;
    vec2 _unused1;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint DEPTH_TEX_SLOT = 4;
const uint NORM_TEX_SLOT = 5;
const uint DEPTH_HIST_TEX_SLOT = 6;
const uint NORM_HIST_TEX_SLOT = 7;
const uint GI_TEX_SLOT = 8;
const uint GI_HIST_TEX_SLOT = 9;
const uint VELOCITY_TEX_SLOT = 10;
const uint VARIANCE_HIST_TEX_SLOT = 11;
const uint SAMPLE_COUNT_HIST_TEX_SLOT = 12;
const uint TILE_LIST_BUF_SLOT = 13;

const uint OUT_REPROJECTED_IMG_SLOT = 0;
const uint OUT_AVG_GI_IMG_SLOT = 1;
const uint OUT_VARIANCE_IMG_SLOT = 2;
const uint OUT_SAMPLE_COUNT_IMG_SLOT = 3;

INTERFACE_END

#endif // RT_DIFFUSE_REPROJECT_INTERFACE_H