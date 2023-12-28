#ifndef SW_ZBUFFER_H
#define SW_ZBUFFER_H

#include "SWcore.h"

typedef struct SWzrange {
    SWfloat min, max;
} SWzrange;

typedef struct SWzbuffer {
    SWint w, h;
    SWfloat *depth;
    SWint tile_w, tile_h;
    SWzrange *tiles;
    SWfloat zmax;
} SWzbuffer;

typedef enum SWoccresult { SW_OCCLUDED = 0, SW_NONOCCLUDED, SW_PARTIAL } SWoccresult;

void swZbufInit(SWzbuffer *zb, SWint w, SWint h, SWfloat zmax);
void swZbufDestroy(SWzbuffer *zb);

void swZbufClearDepth(SWzbuffer *zb, SWfloat val);

#define swZbufSetDepth(zb, x, y, val) (zb)->depth[(y) * (zb)->w + (x)] = (val)
#define swZbufGetDepth(zb, x, y) (zb)->depth[(y) * (zb)->w + (x)]
#define swZbufTestDepth(zb, x, y, z) ((z) <= (zb)->depth[(y) * (zb)->w + (x)])

#define swZbufSetTileRange(zb, x, y, zmin, zmax)                                         \
    {                                                                                    \
        SWzrange *_zr =                                                                  \
            &(zb)->tiles[((y) / SW_TILE_SIZE) * (zb)->tile_w + ((x) / SW_TILE_SIZE)];    \
        _zr->min = (zmin);                                                               \
        _zr->max = (zmax);                                                               \
    }

#define swZbufUpdateTileRange(zb, x, y, zmin, zmax)                                      \
    {                                                                                    \
        SWzrange *_zr =                                                                  \
            &(zb)->tiles[((y) / SW_TILE_SIZE) * (zb)->tile_w + ((x) / SW_TILE_SIZE)];    \
        _zr->min = sw_min((zmin), _zr->min);                                             \
        _zr->max = sw_max((zmax), _zr->max);                                             \
    }

sw_inline SWoccresult swZbufTestTileRange(const SWzbuffer *zb, SWint x, SWint y,
                                          SWfloat min, SWfloat max) {
    SWzrange *zr = &zb->tiles[(y / SW_TILE_SIZE) * zb->tile_w + (x / SW_TILE_SIZE)];
    if (max < zr->min) {
        return SW_NONOCCLUDED;
    } else if (min > zr->max) {
        return SW_OCCLUDED;
    } else {
        return SW_PARTIAL;
    }
}

sw_inline SWzrange *swZbufGetTileRange(const SWzbuffer *zb, SWint x, SWint y) {
    return &zb->tiles[(y / SW_TILE_SIZE) * zb->tile_w + (x / SW_TILE_SIZE)];
}

void swZbufSetDepth_(SWzbuffer *zb, SWint x, SWint y, SWfloat val);
SWint swZbufTestDepth_(SWzbuffer *zb, SWint x, SWint y, SWfloat z);

SWoccresult swZbufTriTestDepth(SWzbuffer *zb, SWint min[2], SWint max[2], SWfloat *attrs1,
                               SWfloat *attrs2, SWfloat *attrs3);

#endif /* SW_ZBUFFER_H */
