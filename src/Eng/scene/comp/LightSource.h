#pragma once

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
enum class eLightType { Point, Sphere, Rect, Disk, Line, _Count };

struct LightSource {
    eLightType type;

    Ren::Vec3f offset;
    float radius;
    Ren::Vec3f col;
    float brightness;
    Ren::Vec3f dir;
    float spot, cap_radius;
    float cull_offset, cull_radius;
    bool cast_shadow, cache_shadow;
    float shadow_bias[2];

    float width, height;
    float area;

    Ren::Vec3f bbox_min;
    float angle_deg;
    Ren::Vec3f bbox_max;

    static void Read(const JsObjectP &js_in, LightSource &ls);
    static void Write(const LightSource &ls, JsObjectP &js_out);

    static const char *name() { return "light"; }
};
} // namespace Eng
