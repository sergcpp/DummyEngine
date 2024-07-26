#pragma once

#include <string_view>

#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
enum class eLightType { Sphere, Rect, Disk, Line, _Count };

struct LightSource {
    eLightType type;

    Ren::Vec3f offset;
    float radius;
    Ren::Vec3f col;
    float power;
    Ren::Vec3f dir;
    float spot_angle, spot_blend, cap_radius;
    float spot_cos;
    float cull_offset, cull_radius;
    bool sky_portal = false;
    bool cast_shadow, cache_shadow;
    bool affect_diffuse = true, affect_specular = true;
    float shadow_bias[2];

    float width, height;
    float area;

    Ren::Vec3f bbox_min;
    float angle_deg;
    Ren::Vec3f bbox_max;

    static void Read(const JsObjectP &js_in, LightSource &ls);
    static void Write(const LightSource &ls, JsObjectP &js_out);

    static std::string_view name() { return "light"; }
};
} // namespace Eng
