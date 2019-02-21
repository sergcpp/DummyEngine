#ifndef SW_FRAMEBUFFER_H
#define SW_FRAMEBUFFER_H

#include "SWcore.h"
#include "SWpixels.h"
#include "SWzbuffer.h"

typedef struct SWframebuffer {
    SWenum type;
    SWint w, h;
    void *pixels;
    SWzbuffer *zbuf;
} SWframebuffer;

struct SWtexture;

void swFbufInit(SWframebuffer *f, SWenum type, SWint w, SWint h, SWint with_depth);
void swFbufDestroy(SWframebuffer *f);

void swFbufClearColor_RGBA(SWframebuffer *f, SWubyte *rgba);
void swFbufClearColorFloat(SWframebuffer *f, SWfloat r, SWfloat g, SWfloat b, SWfloat a);

#define swFbufSetPixel_FRGBA(f, x, y, col)                                              \
    if ((f)->type == SW_BGRA8888) {                                                     \
        swPx_BGRA8888_SetColor_FRGBA((f)->w, (f)->h, (f)->pixels, (x), (y), (col));     \
    } else if ((f)->type == SW_FRGBA) {                                                 \
        swPx_FRGBA_SetColor_FRGBA((f)->w, (f)->h, (f)->pixels, (x), (y), (col));        \
    }

#define swFbufSetPixel_BGRA8888(f, x, y, col)                                           \
    if ((f)->type == SW_BGRA8888) {                                                     \
        swPx_BGRA8888_SetColor_BGRA8888((f)->w, (f)->h, (f)->pixels, (x), (y), (col));  \
    }

#define swFbufGetPixel_FRGBA(f, x, y, col)                                              \
    if ((f)->type == SW_BGRA8888) {                                                     \
        swPx_BGRA8888_GetColor_FRGBA((f)->w, (f)->h, (f)->pixels, (x), (y), (col));     \
    } else { (col)[0] = (col)[1] = (col)[2] = (col)[3] = 0; }

#define swFbufTestDepth(f, x, y, z) \
    swZbufTestDepth((f)->zbuf, x, y, z)

#define swFbufSetDepth(f, x, y, z)  \
    swZbufSetDepth((f)->zbuf, x, y, z)


#define swFbufSetTileRange(f, x, y, zmin, zmax) \
    swZbufSetTileRange((f)->zbuf, x, y, zmin, zmax)

#define swFbufUpdateTileRange(f, x, y, zmin, zmax)  \
    swZbufUpdateTileRange((f)->zbuf, x, y, zmin, zmax)

#define swFbufTestTileRange(f, x, y, zmin, zmax)    \
    swZbufTestTileRange((f)->zbuf, x, y, zmin, zmax)


#define swFbufClearDepth(f, z) \
    swZbufClearDepth((f)->zbuf, (z))

#define swFbufBGRA8888_SetPixel_FRGBA(f, x, y, col) \
    swPx_BGRA8888_SetColor_FRGBA((f)->w, (f)->h, (f)->pixels, (x), (y), (col))

void swFbufBlitPixels(SWframebuffer *f, SWint x, SWint y, SWint pitch, SWenum type, SWenum mode, SWint w, SWint h, const void *pixels, SWfloat scale);
void swFbufBlitTexture(SWframebuffer *f, SWint x, SWint y, const struct SWtexture *t, SWfloat scale);

/*static sw_inline SWint swFbufTestDepth(SWframebuffer *f, SWint x, SWint y, SWfloat z) {
    return !(z > f->depth[y * f->w + x]);
}*/

#endif /* SW_FRAMEBUFFER_H */