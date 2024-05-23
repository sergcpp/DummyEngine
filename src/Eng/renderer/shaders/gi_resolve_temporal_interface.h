#ifndef GI_RESOLVE_TEMPORAL_INTERFACE_H
#define GI_RESOLVE_TEMPORAL_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(GIResolveTemporal)

struct Params {
    uvec2 img_size;
};

const int LOCAL_GROUP_SIZE_X = 8;
const int LOCAL_GROUP_SIZE_Y = 8;

const int NORM_TEX_SLOT = 2;
const int AVG_GI_TEX_SLOT = 3;
const int GI_TEX_SLOT = 4;
const int REPROJ_GI_TEX_SLOT = 5;
const int VARIANCE_TEX_SLOT = 6;
const int SAMPLE_COUNT_TEX_SLOT = 7;
const int TILE_LIST_BUF_SLOT = 8;
const int EXPOSURE_TEX_SLOT = 9;

const int OUT_GI_IMG_SLOT = 0;
const int OUT_VARIANCE_IMG_SLOT = 1;

INTERFACE_END

#endif // GI_RESOLVE_TEMPORAL_INTERFACE_H