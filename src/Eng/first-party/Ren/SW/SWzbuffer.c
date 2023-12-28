#include "SWzbuffer.h"

#include <stdlib.h>
#include <string.h>

#define _swGetTile(zb, x, y)                                                             \
    &zb->tiles[(y & ~(SW_TILE_SIZE - 1) * zb->tile_w) + (x & ~(SW_TILE_SIZE - 1))]

void swZbufInit(SWzbuffer *zb, const SWint w, const SWint h, const SWfloat zmax) {
    memset(zb, 0, sizeof(SWzbuffer));
    zb->w = w;
    zb->h = h;
    zb->tile_w = (w + (SW_TILE_SIZE - 1)) / SW_TILE_SIZE;
    zb->tile_h = (h + (SW_TILE_SIZE - 1)) / SW_TILE_SIZE;
    zb->depth = (SWfloat *)malloc(sizeof(SWfloat) * w * h);
    zb->tiles = (SWzrange *)malloc(sizeof(SWzrange) * zb->tile_w * zb->tile_h);
    zb->zmax = zmax;
    swZbufClearDepth(zb, 1);
}

void swZbufDestroy(SWzbuffer *zb) {
    free(zb->depth);
    free(zb->tiles);
    memset(zb, 0, sizeof(SWzbuffer));
}

void swZbufClearDepth(SWzbuffer *zb, const SWfloat val) {
    SWint i;
    for (i = 0; i < zb->w; i++) {
        zb->depth[i] = val;
    }
    for (i = 1; i < zb->h; i++) {
        memcpy(&zb->depth[i * zb->w], zb->depth, sizeof(SWfloat) * zb->w);
    }

    for (i = 0; i < zb->tile_w; i++) {
        zb->tiles[i].min = zb->tiles[i].max = val;
    }

    for (i = 1; i < zb->tile_h; i++) {
        memcpy(&zb->tiles[i * zb->tile_w], zb->tiles, sizeof(SWzrange) * zb->tile_w);
    }
}

/* Without tiles */
#if 1
void swZbufSetDepth_(SWzbuffer *zb, const SWint x, const SWint y, const SWfloat val) {
    const SWint index = y * zb->w + x;
    zb->depth[index] = val;
}
SWint swZbufTestDepth_(SWzbuffer *zb, const SWint x, const SWint y, const SWfloat z) {
    return z <= zb->depth[y * zb->w + x];
}
#else

#include <assert.h>

void swZbufSetDepth_(SWzbuffer *zb, SWint x, SWint y, SWfloat val) {
    SWzrange *r = _swGetTile(zb, x, y);
    SWint i, j;
    SWint index = y * zb->w + x;
    /*SWint iiii = y & ~(SW_TILE_SIZE - 1) * zb->tile_w + (x & ~(SW_TILE_SIZE - 1));*/
    /*SWint tile_index1 = y & ~(SW_TILE_SIZE - 1) * zb->tile_w + (x & ~(SW_TILE_SIZE -
    1)); SWint tile_index = index / (SW_TILE_SIZE * SW_TILE_SIZE); assert(tile_index1 ==
    tile_index); SWzrange *r = &zb->tiles[tile_index];*/
    zb->depth[index] = val;
    if (val < r->min) {
        r->min = val;
        r->min_index = index;
    } else if (index == r->min_index && val != r->min) {
        r->min = val;
        goto RECALC;
    }

    if (val > r->max) {
        r->max = val;
        r->max_index = index;
    } else if (index == r->min_index && val != r->max) {
        r->max = val;
        goto RECALC;
    }
    return;

RECALC:
    x &= ~(SW_TILE_SIZE - 1);
    y &= ~(SW_TILE_SIZE - 1);

    for (i = 0; i < SW_TILE_SIZE; i++) {
        for (j = 0; j < SW_TILE_SIZE; j++) {
            SWint ind = (y + i) * zb->w + (x + j);
            /*SWint tile_ind = ind & ~(SW_TILE_SIZE - 1);*/
            if (zb->depth[ind] < r->min) {
                r->min = zb->depth[ind];
                r->min_index = ind;
            } else if (zb->depth[ind] > r->max) {
                r->max = zb->depth[ind];
                r->max_index = ind;
            }
        }
    }
}

SWint swZbufTestDepth_(SWzbuffer *zb, SWint x, SWint y, SWfloat z) {
    SWzrange *r = _swGetTile(zb, x, y);
    if (z < r->max) {
        if (z < r->min) {
            r->min = z;
            return 1;
        }
        return z < zb->depth[y * zb->w + x];
    } else {
        r->max = z;
    }
    return 0;
}

#endif

SWoccresult swZbufTriTestDepth(SWzbuffer *zb, SWint min[2], SWint max[2], SWfloat *attrs1,
                               SWfloat *attrs2, SWfloat *attrs3) {
    SWfloat /*tri_min_z, */ tri_max_z, buf_min_z = 1, buf_max_z = 0;
    SWint i, j;
    /*tri_min_z = sw_min(attrs1[2], sw_min(attrs2[2], attrs3[2]));*/
    tri_max_z = sw_max(attrs1[2], sw_max(attrs2[2], attrs3[2]));

    for (j = min[1] / SW_TILE_SIZE; j < max[1] / SW_TILE_SIZE; j++) {
        for (i = min[0] / SW_TILE_SIZE; i < max[0] / SW_TILE_SIZE; i++) {
            SWzrange *r = &zb->tiles[j * zb->tile_w + i];
            buf_min_z = sw_min(buf_min_z, r->min);
            buf_max_z = sw_max(buf_max_z, r->max);
        }
    }

    if (tri_max_z < buf_min_z) {
        return SW_NONOCCLUDED;
    } else {
        return SW_PARTIAL;
    }
}
