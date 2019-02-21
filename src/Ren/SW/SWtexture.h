#ifndef SW_TEXTURE_H
#define SW_TEXTURE_H

#include "SWcore.h"
#include "SWpixels.h"

typedef struct SWtexture {
    SWenum mode, type;
    SWint w, h, type_size, pp_size;
    void *pixels;
    void(*free)(void *);
} SWtexture;

extern SWfloat _sw_ubyte_to_float_table[256];

void swTexInit(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, const void *pixels);
void swTexInitMove(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, void *pixels, void(*free)(void *));
void swTexInitMove_malloced(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, void *pixels);
void swTexDestroy(SWtexture *t);

#define swTex_RGB888_GetColorFloat_RGBA(t, u, v, rgba) \
    swPx_RGB888_GetColor_FRGBA_UV((t)->w, (t)->h, (t)->pixels, u, v, rgba) \

#define swTexGetColorFloat_RGBA(t, u, v, rgba) {        \
    SWint x = (SWint)(u * (t)->w) & ((t)->w - 1);       \
    SWint y = (SWint)(v * (t)->h) & ((t)->h - 1);       \
                                                        \
    switch ((t)->type) {                                \
        case SW_UNSIGNED_BYTE:{                         \
            SWint i;                                    \
            SWubyte *p = (SWubyte*)(t)->pixels;         \
            if ((t)->mode == SW_RGB) {                  \
                p += 3 * (y * (t)->w + x);              \
                rgba[3] = (SWfloat)1.0;                 \
            } else if ((t)->mode == SW_RGBA) {          \
                p += 4 * (y * (t)->w + x);              \
                rgba[3] = _sw_ubyte_to_float_table[p[3]];\
            }                                           \
                                                        \
            for (i = 0; i < 3; i++) {                   \
                rgba[i] = _sw_ubyte_to_float_table[p[i]];\
            }                                           \
        }break;                                         \
        case SW_COMPRESSED:{                            \
            const SWubyte *table = (const SWubyte *)(t)->pixels;                \
            const SWubyte *pixels = (const SWubyte *)(t)->pixels + 256 * 4 * 4; \
                                                                                \
            SWint i = 4 * (2 * (y & 1) + (x & 1));                              \
            x >>= 1; y >>= 1;                                                   \
            const SWubyte index = pixels[y * ((t)->w >> 1) + x];                \
            const SWubyte *_col = &table[index * 16 + i];                       \
                                                                                \
            for (i = 0; i < 4; i++) {                                           \
                rgba[i] = _sw_ubyte_to_float_table[_col[i]];                    \
            }                                           \
        }break;                                         \
        default:                                        \
            break;                                      \
    }                                                   \
}

#define swTexGetColorUbyte_RGBA(t, u, v, rgba) swPxGetColorUbyte_RGBA(t->type, t->mode, t->w, t->h, t->pixels, u, v, rgba)
#define swTexGetColorUbyte_BGRA(t, u, v, bgra) swPxGetColorUbyte_BGRA(t->type, t->mode, t->w, t->h, t->pixels, u, v, bgra)

#endif /* SW_TEXTURE_H */
