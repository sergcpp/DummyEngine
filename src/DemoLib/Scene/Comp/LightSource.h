#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightSource {
    Ren::Vec3f  offset;
    float       radius;
    Ren::Vec3f  col;
    float       brightness;
    Ren::Vec3f  dir;
    float       spot;
    float       influence;
    bool        cast_shadow, cache_shadow;

    Ren::Vec3f bbox_min, bbox_max;

    static void Read(const JsObject &js_in, LightSource &ls);
    static void Write(const LightSource &ls, JsObject &js_out);

    static const char *name() { return "light"; }
};