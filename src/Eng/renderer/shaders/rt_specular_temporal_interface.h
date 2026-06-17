#ifndef RT_SPECULAR_TEMPORAL_INTERFACE_H
#define RT_SPECULAR_TEMPORAL_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(RTSpecularResolveTemporal)

struct Params {
    uvec2 img_size;
    vec2 thresholds;
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint NORM_TEX_SLOT = 2;
const uint AVG_REFL_TEX_SLOT = 3;
const uint REFL_TEX_SLOT = 4;
const uint REPROJ_REFL_TEX_SLOT = 5;
const uint VARIANCE_TEX_SLOT = 6;
const uint SAMPLE_COUNT_TEX_SLOT = 7;
const uint TILE_LIST_BUF_SLOT = 8;

const uint OUT_REFL_IMG_SLOT = 0;
const uint OUT_VARIANCE_IMG_SLOT = 1;

INTERFACE_END

#endif // RT_SPECULAR_TEMPORAL_INTERFACE_H