#include "SWtexture.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "SWcontext.h"

SWfloat _sw_ubyte_to_float_table[256];

sw_inline void _swGetSizes(SWenum mode, SWenum type, SWint *t_size, SWint *p_size, SWint *additional) {
    SWint type_size, pp_size;
    switch (type) {
    case SW_UNSIGNED_BYTE:
        type_size = 1;
        break;
    case SW_UNSIGNED_SHORT:
        type_size = 2;
        break;
    case SW_UNSIGNED_INT:
        type_size = 4;
        break;
    case SW_COMPRESSED:
        type_size = 0;
        if (additional) *additional = 256 * 4 * 4;
        break;
    default:
        type_size = 0;
        assert(0);
    }
    switch (mode) {
    case SW_RGB:
        pp_size = type_size * 3;
        break;
    case SW_RGBA:
        pp_size = type_size * 4;
        break;
    default:
        pp_size = 0;
        assert(0);
    }

    if (t_size) (*t_size) = type_size;
    if (p_size) (*p_size) = pp_size;
}

void swTexInit(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, const void *pixels) {
    SWint pp_size, additional = 0;
    _swGetSizes(mode, type, NULL, &pp_size, &additional);
    size_t total_size = (size_t)pp_size * w * h + additional;
    if (type == SW_COMPRESSED) {
        total_size += w * h / 4;
    }
    void *p = malloc(total_size);
    memcpy(p, pixels, total_size);
    swTexInitMove_malloced(t, mode, type, w, h, p);
}

void swTexInitMove(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, void *pixels, void(*free)(void *)) {
    assert(w == 1 || w % 2 == 0);
    assert(h == 1 || h % 2 == 0);
    t->mode = mode;
    t->type = type;
    t->pixels = pixels;
    t->w = w;
    t->h = h;
    t->free = free;

    if (type == SW_COMPRESSED) {
        assert(w > 1 && h > 1);
    }

    _swGetSizes(mode, type, &t->type_size, &t->pp_size, NULL);
}

void swTexInitMove_malloced(SWtexture *t, SWenum mode, SWenum type, SWint w, SWint h, void *pixels) {
    swTexInitMove(t, mode, type, w, h, pixels, &free);
}

void swTexDestroy(SWtexture *t) {
    if (t->free) {
        (*t->free)(t->pixels);
    }
    memset(t, 0, sizeof(SWtexture));
}

/*void swTexGetColorFloat_RGBA(SWtexture *t, SWfloat u, SWfloat v, SWfloat *rgba) {
    SWint x = (SWint)(u * t->w) & (t->w - 1);
    SWint y = (SWint)(v * t->h) & (t->h - 1);

    switch (t->type) {
        case SW_UNSIGNED_BYTE:{
            const SWfloat conv = (SWfloat)1.0 / 255;
            SWubyte *p = t->pixels;
            if (t->mode == SW_RGB) {
                p += 3 * (y * t->w + x);
                rgba[3] = (SWfloat)1.0;
            } else if (t->mode == SW_RGBA) {
                p += 4 * (y * t->w + x);
                rgba[3] = p[3] * conv;
            }
            rgba[0] = p[0] * conv;
            rgba[1] = p[1] * conv;
            rgba[2] = p[2] * conv;
        }break;
    }
}*/
