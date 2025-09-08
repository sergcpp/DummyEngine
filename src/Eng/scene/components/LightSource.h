#pragma once

#include <string_view>

#include <Ren/Bitmask.h>
#include <Ren/MMat.h>

#include "Common.h"

namespace Eng {
enum class eLightType : uint8_t { Sphere, Rect, Disk, Line, _Count };
enum class eLightFlags : uint8_t { SkyPortal, CastShadow, AffectDiffuse, AffectSpecular, AffectRefraction, AffectVolume };

struct LightSource {
    eLightType type;
    Ren::Bitmask<eLightFlags> flags;

    Ren::Vec3f offset;
    float radius;
    Ren::Vec3f col;
    float power;
    Ren::Vec3f dir;
    float spot_angle, spot_blend, cap_radius;
    float spot_cos;
    float cull_offset, cull_radius;
    float shadow_bias[2];

    float width, height;
    float area, _radius;

    Ren::Vec3f bbox_min;
    float angle_deg;
    Ren::Vec3f bbox_max;
    float spread_deg;

    static void Read(const Sys::JsObjectP &js_in, LightSource &ls);
    static void Write(const LightSource &ls, Sys::JsObjectP &js_out);

    static std::string_view name() { return "light"; }
};
} // namespace Eng
