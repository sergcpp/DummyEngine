#include "test_common.h"

#include <math.h>

#include "../SWzbuffer.h"

static const int RES_W = 50;
static const int RES_H = 16;

void test_zbuffer() {
    const SWfloat eps = (SWfloat)0.0001;

#define TEST_BEGIN                  \
    SWzbuffer zb_;                  \
    swZbufInit(&zb_, RES_W, RES_H, 1.0f); \
    require(zb_.depth != NULL);     \
    require(zb_.depth[0] == 1);

#define TEST_END                    \
    swZbufDestroy(&zb_);            \
    require(zb_.depth == NULL);

    {
        // Zbuffer swZbufClearDepth
        TEST_BEGIN

        swZbufClearDepth(&zb_, 0.5f);
        for (int i = 0; i < zb_.w * zb_.h; i++) {
            require(zb_.depth[i] == 0.5f);
        }
        for (int i = 0; i < zb_.tile_w * zb_.tile_h; i++) {
            require(zb_.tiles[i].min == 0.5f);
            require(zb_.tiles[i].max == 0.5f);
        }

        TEST_END
    }

    {
        // Zbuffer swZbufTestDepth
        TEST_BEGIN

        for (int j = 0; j < RES_H; j++) {
            for (int i = 0; i < RES_W; i++) {
                swZbufSetDepth(&zb_, i, j, 100 * 0.01f * i + j * 0.01f);
            }
        }


        for (int j = 0; j < RES_H; j++) {
            for (int i = 0; i < RES_W; i++) {
                SWfloat z = (100 * 0.01f * i + j * 0.01f);
                require(fabs(swZbufGetDepth(&zb_, i, j) - z) < eps);
                require(!swZbufTestDepth(&zb_, i, j, z + 0.01f));
                require(swZbufTestDepth(&zb_, i, j, z - 0.01f));
            }
        }

        TEST_END
    }

    {
        // Zbuffer swZbufTestTile
        TEST_BEGIN

        for (int j = 0; j < RES_H; j += SW_TILE_SIZE) {
            for (int i = 0; i < RES_W; i += SW_TILE_SIZE) {
                SWfloat min = i * 0.4f + j * 0.6f;
                SWfloat max = min + 0.15f;

                swZbufSetTileRange(&zb_, i, j, min, max);
            }
        }

        for (int j = 0; j < RES_H; j += SW_TILE_SIZE) {
            for (int i = 0; i < RES_W; i += SW_TILE_SIZE) {
                SWfloat min = i * 0.4f + j * 0.6f;
                SWfloat max = min + 0.15f;

                require(swZbufTestTileRange(&zb_, i, j, max + 0.1f, max + 0.2f) == SW_OCCLUDED);
                require(swZbufTestTileRange(&zb_, i, j, min - 0.5f, min - 0.2f) == SW_NONOCCLUDED);
                require(swZbufTestTileRange(&zb_, i, j, min - 0.2f, min + 0.2f) == SW_PARTIAL);
                require(swZbufTestTileRange(&zb_, i, j, max - 0.2f, max + 0.2f) == SW_PARTIAL);
            }
        }

        TEST_END
    }
}
