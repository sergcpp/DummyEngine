#ifndef SWPIXELS_H
#define SWPIXELS_H

#include "SWcore.h"
#include "SWtypes.h"

#define UBYTE_TO_FLOAT(x) ((x) * ((SWfloat)1.0 / 255))
#define FLOAT_TO_UBYTE(x) (SWubyte)((x) * ((SWfloat)255.999))

/* Read operations */

#define _swPx_RGB888_GetColor_RGBA8888(p, rgba)     \
    rgba[3] = 255;                                  \
    rgba[0] = ((SWubyte*)p)[0];                     \
    rgba[1] = ((SWubyte*)p)[1];                     \
    rgba[2] = ((SWubyte*)p)[2];

#define _swPx_RGBA8888_GetColor_RGBA8888(p, rgba)   \
    rgba[0] = ((SWubyte*)p)[0];                     \
    rgba[1] = ((SWubyte*)p)[1];                     \
    rgba[2] = ((SWubyte*)p)[2];                     \
    rgba[3] = ((SWubyte*)p)[3];

#define _swPx_RGB888_GetColor_FRGBA(p, rgba)        \
    rgba[0] = UBYTE_TO_FLOAT(((SWubyte*)p)[0]);     \
    rgba[1] = UBYTE_TO_FLOAT(((SWubyte*)p)[1]);     \
    rgba[2] = UBYTE_TO_FLOAT(((SWubyte*)p)[2]);     \
    rgba[3] = (SWfloat)1.0;

#define _swPx_RGBA8888_GetColor_FRGBA(p, rgba)      \
    rgba[0] = UBYTE_TO_FLOAT(((SWubyte*)p)[0]);     \
    rgba[1] = UBYTE_TO_FLOAT(((SWubyte*)p)[1]);     \
    rgba[2] = UBYTE_TO_FLOAT(((SWubyte*)p)[2]);     \
    rgba[3] = UBYTE_TO_FLOAT(((SWubyte*)p)[3]);

#define _swPx_BGRA8888_GetColor_FRGBA(p, rgba)      \
    rgba[0] = UBYTE_TO_FLOAT(((SWubyte*)p)[2]);     \
    rgba[1] = UBYTE_TO_FLOAT(((SWubyte*)p)[1]);     \
    rgba[2] = UBYTE_TO_FLOAT(((SWubyte*)p)[0]);     \
    rgba[3] = UBYTE_TO_FLOAT(((SWubyte*)p)[3]);     \

#define _swPx_RGB888_GetColor_BGRA8888(p, bgra)     \
    bgra[3] = 255;                                  \
    bgra[0] = ((SWubyte*)p)[2];                     \
    bgra[1] = ((SWubyte*)p)[1];                     \
    bgra[2] = ((SWubyte*)p)[0];

#define _swPx_RGBA8888_GetColor_BGRA8888(p, bgra)   \
    bgra[0] = ((SWubyte*)p)[2];                     \
    bgra[1] = ((SWubyte*)p)[1];                     \
    bgra[2] = ((SWubyte*)p)[0];                     \
    bgra[3] = ((SWubyte*)p)[3];

#define _swPx_RGB888_GetColor_FBGRA(p, rgba)        \
    rgba[0] = UBYTE_TO_FLOAT(((SWubyte*)p)[2]);     \
    rgba[1] = UBYTE_TO_FLOAT(((SWubyte*)p)[1]);     \
    rgba[2] = UBYTE_TO_FLOAT(((SWubyte*)p)[0]);     \
    rgba[3] = (SWfloat)1.0;

#define _swPx_RGBA8888_GetColor_FBGRA(p, rgba)      \
    rgba[0] = UBYTE_TO_FLOAT(((SWubyte*)p)[2]);     \
    rgba[1] = UBYTE_TO_FLOAT(((SWubyte*)p)[1]);     \
    rgba[2] = UBYTE_TO_FLOAT(((SWubyte*)p)[0]);     \
    rgba[3] = UBYTE_TO_FLOAT(((SWubyte*)p)[3]);


#define swPx_RGB888_GetColor_RGBA8888(w, h, pixels, x, y, rgba) \
    _swPx_RGB888_GetColor_RGBA8888((SWubyte*)(pixels) + 3 * ((y) * (w) + (x)), (rgba))

#define swPx_RGBA8888_GetColor_RGBA8888(w, h, pixels, x, y, rgba) \
    _swPx_RGBA8888_GetColor_RGBA8888((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_RGB888_GetColor_FRGBA(w, h, pixels, x, y, rgba) \
    _swPx_RGB888_GetColor_FRGBA((SWubyte*)(pixels) + 3 * ((y) * (w) + (x)), (rgba))

#define swPx_RGBA8888_GetColor_FRGBA(w, h, pixels, x, y, rgba) \
    _swPx_RGBA8888_GetColor_FRGBA((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_BGRA8888_GetColor_FRGBA(w, h, pixels, x, y, rgba) \
    _swPx_BGRA8888_GetColor_FRGBA((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_RGB888_GetColor_BGRA8888(w, h, pixels, x, y, rgba) \
    _swPx_RGB888_GetColor_BGRA8888((SWubyte*)(pixels) + 3 * ((y) * (w) + (x)), (rgba))

#define swPx_RGBA8888_GetColor_BGRA8888(w, h, pixels, x, y, rgba) \
    _swPx_RGBA8888_GetColor_BGRA8888((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_RGB888_GetColor_FBGRA(w, h, pixels, x, y, rgba) \
    _swPx_RGB888_GetColor_FBGRA((SWubyte*)(pixels) + 3 * ((y) * (w) + (x)), (rgba))

#define swPx_RGBA8888_GetColor_FBGRA(w, h, pixels, x, y, rgba) \
    _swPx_RGBA8888_GetColor_FBGRA((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

/* wrong negative rounding but ok */
#define swPx_RGB888_GetColor_RGBA8888_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGB888_GetColor_RGBA8888((SWubyte*)(pixels) + 3 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))

#define swPx_RGBA8888_GetColor_RGBA8888_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGBA8888_GetColor_RGBA8888((SWubyte*)(pixels) + 4 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))

#define swPx_RGB888_GetColor_FRGBA_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGB888_GetColor_FRGBA((SWubyte*)(pixels) + 3 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))

#define swPx_RGBA8888_GetColor_FRGBA_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGBA8888_GetColor_FRGBA((SWubyte*)(pixels) + 4 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))

#define swPx_RGB888_GetColor_BGRA8888_UV(w, h, pixels, u, v, bgra) \
    _swPx_RGB888_GetColor_BGRA8888((SWubyte*)(pixels) + 3 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (bgra))

#define swPx_RGB888_GetColor_BGRA8888_UV_norepeat_unsafe(w, h, pixels, u, v, bgra) \
    _swPx_RGB888_GetColor_BGRA8888((SWubyte*)(pixels) + 3 * ((SWint)((v) * (h)) * (w) + (SWint)((u) * (w))), (bgra))

#define swPx_RGBA8888_GetColor_BGRA8888_UV(w, h, pixels, u, v, bgra) \
    _swPx_RGBA8888_GetColor_BGRA8888((SWubyte*)(pixels) + 4 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (bgra))

#define swPx_RGBA8888_GetColor_BGRA8888_UV_norepeat_unsafe(w, h, pixels, u, v, bgra) \
    _swPx_RGBA8888_GetColor_BGRA8888((SWubyte*)(pixels) + 4 * ((SWint)((v) * (h)) * (w) + (SWint)((u) * (w))), (bgra))

#define swPx_RGB888_GetColor_FBGRA_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGB888_GetColor_FBGRA((SWubyte*)(pixels) + 3 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))

#define swPx_RGBA8888_GetColor_FBGRA_UV(w, h, pixels, u, v, rgba) \
    _swPx_RGBA8888_GetColor_FBGRA((SWubyte*)(pixels) + 4 * (((SWint)((v) * (h)) & ((h) - 1)) * (w) + ((SWint)((u) * (w)) & ((w) - 1))), (rgba))


#define swPxGetColorUbyte_RGBA(type, mode, w, h, pixels, u, v, rgba)        \
    if (type == SW_UNSIGNED_BYTE) {                                         \
        if (mode == SW_RGB) {                                               \
            swPx_RGB888_GetColor_RGBA8888_UV(w, h, pixels, u, v, rgba);     \
        } else if (mode == SW_RGBA) {                                       \
            swPx_RGBA8888_GetColor_RGBA8888_UV(w, h, pixels, u, v, rgba);   \
        }                                                                   \
    }

#define swPxGetColorUbyte_BGRA(type, mode, w, h, pixels, u, v, bgra)        \
    if (type == SW_UNSIGNED_BYTE) {                                         \
        if (mode == SW_RGB) {                                               \
            swPx_RGB888_GetColor_BGRA8888_UV(w, h, pixels, u, v, bgra);     \
        } else if (mode == SW_RGBA) {                                       \
            swPx_RGBA8888_GetColor_BGRA8888_UV(w, h, pixels, u, v, bgra);   \
        }                                                                   \
    }

#define swPxGetColorFloat_RGBA(type, mode, w, h, pixels, u, v, rgba)        \
    if (type == SW_UNSIGNED_BYTE) {                                         \
        if (mode == SW_RGB) {                                               \
            swPx_RGB888_GetColor_FRGBA_UV(w, h, pixels, u, v, rgba);        \
        } else if (mode == SW_RGBA) {                                       \
            swPx_RGBA8888_GetColor_FRGBA_UV(w, h, pixels, u, v, rgba);      \
        }                                                                   \
    }

#define swPxGetColorFloat_BGRA(type, mode, w, h, pixels, u, v, bgra)        \
    if (type == SW_UNSIGNED_BYTE) {                                         \
        if (mode == SW_RGB) {                                               \
            swPx_RGB888_GetColor_FBGRA_UV(w, h, pixels, u, v, bgra);        \
        } else if (mode == SW_RGBA) {                                       \
            swPx_RGBA8888_GetColor_FBGRA_UV(w, h, pixels, u, v, bgra);      \
        }                                                                   \
    }

/* Write operations */

#define _swPx_RGBA8888_SetColor_FRGBA_(rgba, fr, fg, fb, fa)    \
    ((SWubyte*)(rgba))[0] = FLOAT_TO_UBYTE(fr);                 \
    ((SWubyte*)(rgba))[1] = FLOAT_TO_UBYTE(fg);                 \
    ((SWubyte*)(rgba))[2] = FLOAT_TO_UBYTE(fb);                 \
    ((SWubyte*)(rgba))[3] = FLOAT_TO_UBYTE(fa);

#define _swPx_RGBA8888_SetColor_FRGBA(rgba, frgba)      \
    ((SWubyte*)(rgba))[0] = FLOAT_TO_UBYTE((frgba)[0]); \
    ((SWubyte*)(rgba))[1] = FLOAT_TO_UBYTE((frgba)[1]); \
    ((SWubyte*)(rgba))[2] = FLOAT_TO_UBYTE((frgba)[2]); \
    ((SWubyte*)(rgba))[3] = FLOAT_TO_UBYTE((frgba)[3]);

#define _swPx_BGRA8888_SetColor_RGBA8888(bgra, rgba)    \
    ((SWubyte*)(bgra))[0] = (rgba)[2];                  \
    ((SWubyte*)(bgra))[1] = (rgba)[1];                  \
    ((SWubyte*)(bgra))[2] = (rgba)[0];                  \
    ((SWubyte*)(bgra))[3] = (rgba)[3];

#define _swPx_BGRA8888_SetColor_FRGBA(bgra, frgba)      \
    ((SWubyte*)(bgra))[0] = FLOAT_TO_UBYTE((frgba)[2]); \
    ((SWubyte*)(bgra))[1] = FLOAT_TO_UBYTE((frgba)[1]); \
    ((SWubyte*)(bgra))[2] = FLOAT_TO_UBYTE((frgba)[0]); \
    ((SWubyte*)(bgra))[3] = FLOAT_TO_UBYTE((frgba)[3]);

#define _swPx_BGRA8888_SetColor_BGRA8888(bgra, rgba)    \
    ((SWubyte*)(bgra))[0] = (rgba)[0];                  \
    ((SWubyte*)(bgra))[1] = (rgba)[1];                  \
    ((SWubyte*)(bgra))[2] = (rgba)[2];                  \
    ((SWubyte*)(bgra))[3] = (rgba)[3];

#define _swPx_FRGBA_SetColor_FRGBA(_frgba, frgba)       \
    ((SWfloat*)(_frgba))[0] = (frgba)[0];               \
    ((SWfloat*)(_frgba))[1] = (frgba)[1];               \
    ((SWfloat*)(_frgba))[2] = (frgba)[2];               \
    ((SWfloat*)(_frgba))[3] = (frgba)[3];

#define swPx_BGRA8888_SetColor_RGBA8888(w, h, pixels, x, y, rgba) \
    _swPx_BGRA8888_SetColor_RGBA8888((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_BGRA8888_SetColor_FRGBA(w, h, pixels, x, y, rgba) \
    _swPx_BGRA8888_SetColor_FRGBA((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (rgba))

#define swPx_BGRA8888_SetColor_BGRA8888(w, h, pixels, x, y, bgra) \
    _swPx_BGRA8888_SetColor_BGRA8888((SWubyte*)(pixels) + 4 * ((y) * (w) + (x)), (bgra))

#define swPx_FRGBA_SetColor_FRGBA(w, h, pixels, x, y, rgba) \
    _swPx_FRGBA_SetColor_FRGBA(((SWfloat*)(pixels)) + 4 * ((y) * (w) + (x)), (rgba))

#endif /* SWPIXELS_H */
