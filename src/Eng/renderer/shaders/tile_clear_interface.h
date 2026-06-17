#ifndef TILE_CLEAR_INTERFACE_H
#define TILE_CLEAR_INTERFACE_H

#include "_interface_common.h"

INTERFACE_START(TileClear)

struct Params {
    uint tile_count;
    uint _pad[3];
};

const uint GRP_SIZE_X = 8;
const uint GRP_SIZE_Y = 8;

const uint TILE_LIST_BUF_SLOT = 3;

const uint OUT_RAD_IMG_SLOT = 0;
const uint OUT_AVG_RAD_IMG_SLOT = 1;
const uint OUT_VARIANCE_IMG_SLOT = 2;

INTERFACE_END

#endif // TILE_CLEAR_INTERFACE_H