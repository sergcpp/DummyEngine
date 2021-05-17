#pragma once

#include <Ren/MMat.h>

#include "Common.h"

struct LightSource {
    Ren::Vec3f  offset;
    float       radius;
    Ren::Vec3f  col;
    float       brightness;
    Ren::Vec3f  dir;
    float       spot, cap_radius;
    float       influence;
    bool        cast_shadow, cache_shadow;
    float       shadow_bias[2];

    Ren::Vec3f  bbox_min;
    float       angle_deg;
    Ren::Vec3f  bbox_max;

    static void Read(const JsObjectP &js_in, LightSource &ls);
    static void Write(const LightSource &ls, JsObjectP &js_out);

    static const char *name() { return "light"; }
};