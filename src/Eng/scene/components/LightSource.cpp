#include "LightSource.h"

#include <algorithm>

#include <Sys/Json.h>

#include "../SceneData.h"

namespace Eng::LightSourceInternal {
static const char *g_type_names[] = {"sphere", "rectangle", "disk", "line"};
static_assert(std::size(g_type_names) == int(eLightType::_Count));

float calc_cull_radius(const Ren::Vec3f &col, const float radius) {
    const float brightness = std::max(col[0], std::max(col[1], col[2]));
    float cull_radius = radius * (std::sqrt(brightness / LIGHT_ATTEN_CUTOFF) - 1.0f);
    // TODO: properly determine influence of area lights
    cull_radius *= 10.0f;
    return cull_radius;
}

float default_angle(const eLightType type) {
    float angle_deg = 180.0f;
    if (type == eLightType::Sphere || type == eLightType::Line) {
        // whole sphere
        angle_deg = 360.0f;
    }
    return angle_deg;
}

} // namespace Eng::LightSourceInternal

void Eng::LightSource::Read(const Sys::JsObjectP &js_in, LightSource &ls) {
    using namespace LightSourceInternal;

    ls.type = eLightType::Sphere;

    if (js_in.Has("type")) {
        const Sys::JsStringP &js_type = js_in.at("type").as_str();
        for (int i = 0; i < int(eLightType::_Count); ++i) {
            if (js_type.val == g_type_names[i]) {
                ls.type = eLightType(i);
                break;
            }
        }
    }

    ls.col[0] = ls.col[1] = ls.col[2] = 1.0f;
    if (js_in.Has("color")) {
        const Sys::JsArrayP &js_color = js_in.at("color").as_arr();
        ls.col[0] = float(js_color[0].as_num().val);
        ls.col[1] = float(js_color[1].as_num().val);
        ls.col[2] = float(js_color[2].as_num().val);
    }

    ls.power = 1.0f;
    if (js_in.Has("power")) {
        const Sys::JsNumber &js_power = js_in.at("power").as_num();
        ls.power = float(js_power.val);
    }

    if (js_in.Has("offset")) {
        const Sys::JsArrayP &js_offset = js_in.at("offset").as_arr();

        ls.offset[0] = float(js_offset[0].as_num().val);
        ls.offset[1] = float(js_offset[1].as_num().val);
        ls.offset[2] = float(js_offset[2].as_num().val);
    }

    ls.radius = 1.0f;
    if (js_in.Has("radius")) {
        const Sys::JsNumber &js_radius = js_in.at("radius").as_num();
        ls.radius = float(js_radius.val);
    }
    ls._radius = ls.radius;

    ls.width = 1.0f;
    if (js_in.Has("width")) {
        const Sys::JsNumber &js_width = js_in.at("width").as_num();
        ls.width = float(js_width.val);
    }

    ls.height = 1.0f;
    if (js_in.Has("height")) {
        const Sys::JsNumber &js_height = js_in.at("height").as_num();
        ls.height = float(js_height.val);
    }

    if (ls.type == eLightType::Sphere) {
        ls.area = 4.0f * Ren::Pi<float>() * ls.radius * ls.radius;
    } else if (ls.type == eLightType::Rect) {
        ls.area = ls.width * ls.height;
        // set to diagonal
        ls._radius = std::sqrt(ls.width * ls.width + ls.height * ls.height);
    } else if (ls.type == eLightType::Disk) {
        ls.area = 0.25f * Ren::Pi<float>() * ls.width * ls.height;
        // set to avg dim
        ls._radius = 0.5f * (ls.width + ls.height);
    } else if (ls.type == eLightType::Line) {
        // set to length
        ls._radius = ls.area = ls.height;
    }

    if (js_in.Has("cull_offset")) {
        const Sys::JsNumber &js_cull_offset = js_in.at("cull_offset").as_num();
        ls.cull_offset = float(js_cull_offset.val);
    } else {
        ls.cull_offset = 0.1f;
    }

    if (js_in.Has("cull_radius")) {
        const Sys::JsNumber &js_cull_radius = js_in.at("cull_radius").as_num();
        ls.cull_radius = float(js_cull_radius.val);
    } else {
        ls.cull_radius = calc_cull_radius(ls.col, ls.radius);
    }

    ls.dir[0] = ls.dir[2] = 0.0f;
    ls.dir[1] = -1.0f;
    if (js_in.Has("direction")) {
        const Sys::JsArrayP &js_dir = js_in.at("direction").as_arr();

        ls.dir[0] = float(js_dir[0].as_num().val);
        ls.dir[1] = float(js_dir[1].as_num().val);
        ls.dir[2] = float(js_dir[2].as_num().val);
    }

    ls.angle_deg = default_angle(ls.type);
    if (js_in.Has("spot_angle")) {
        const Sys::JsNumber &js_spot_angle = js_in.at("spot_angle").as_num();
        ls.angle_deg = float(js_spot_angle.val);
    }

    const float angle_rad = ls.angle_deg * Ren::Pi<float>() / 180.0f;

    ls.spot_angle = 0.5f * angle_rad;
    ls.spot_cos = cosf(ls.spot_angle);
    ls.cap_radius = ls.cull_radius * std::tan(angle_rad);

    ls.spot_blend = 0.0f;
    if (js_in.Has("spot_blend")) {
        const Sys::JsNumber &js_spot_blend = js_in.at("spot_blend").as_num();
        ls.spot_blend = float(js_spot_blend.val);
    }

    ls.flags = Ren::Bitmask{eLightFlags::AffectDiffuse} | eLightFlags::AffectSpecular | eLightFlags::AffectRefraction |
               eLightFlags::AffectVolume;

    if (js_in.Has("sky_portal")) {
        const Sys::JsLiteral &js_sky_portal = js_in.at("sky_portal").as_lit();
        if (js_sky_portal.val == Sys::JsLiteralType::True) {
            ls.flags |= eLightFlags::SkyPortal;
        }
    }

    if (js_in.Has("cast_shadow")) {
        if (js_in.at("cast_shadow").as_lit().val == Sys::JsLiteralType::True) {
            ls.flags |= eLightFlags::CastShadow;
        }
    }

    if (js_in.Has("affect_diffuse")) {
        if (js_in.at("affect_diffuse").as_lit().val == Sys::JsLiteralType::False) {
            ls.flags &= ~Ren::Bitmask{eLightFlags::AffectDiffuse};
        }
    }

    if (js_in.Has("affect_specular")) {
        if (js_in.at("affect_specular").as_lit().val == Sys::JsLiteralType::False) {
            ls.flags &= ~Ren::Bitmask{eLightFlags::AffectSpecular};
        }
    }

    if (js_in.Has("affect_refraction")) {
        if (js_in.at("affect_refraction").as_lit().val == Sys::JsLiteralType::False) {
            ls.flags &= ~Ren::Bitmask{eLightFlags::AffectRefraction};
        }
    }

    if (js_in.Has("affect_volume")) {
        if (js_in.at("affect_volume").as_lit().val == Sys::JsLiteralType::False) {
            ls.flags &= ~Ren::Bitmask{eLightFlags::AffectVolume};
        }
    }

    if (js_in.Has("shadow_bias")) {
        const Sys::JsArrayP &js_shadow_bias = js_in.at("shadow_bias").as_arr();
        ls.shadow_bias[0] = float(js_shadow_bias.at(0).as_num().val);
        ls.shadow_bias[1] = float(js_shadow_bias.at(1).as_num().val);
    } else {
        ls.shadow_bias[0] = 4.0f;
        ls.shadow_bias[1] = 8.0f;
    }
}

void Eng::LightSource::Write(const LightSource &ls, Sys::JsObjectP &js_out) {
    using namespace LightSourceInternal;

    const auto &alloc = js_out.elements.get_allocator();

    { // Write type
        Sys::JsStringP js_type(alloc);

        js_type.val = g_type_names[int(ls.type)];

        js_out.Insert("type", std::move(js_type));
    }

    if (ls.col[0] != 1.0f || ls.col[1] != 1.0f || ls.col[2] != 1.0f) { // Write color
        Sys::JsArrayP js_color(alloc);

        js_color.Push(Sys::JsNumber{ls.col[0]});
        js_color.Push(Sys::JsNumber{ls.col[1]});
        js_color.Push(Sys::JsNumber{ls.col[2]});

        js_out.Insert("color", std::move(js_color));
    }

    if (ls.power != 1.0f) {
        js_out.Insert("power", Sys::JsNumber{ls.power});
    }

    if (ls.offset[0] != 0.0f || ls.offset[1] != 0.0f || ls.offset[2] != 0.0f) {
        Sys::JsArrayP js_offset(alloc);

        js_offset.Push(Sys::JsNumber{ls.offset[0]});
        js_offset.Push(Sys::JsNumber{ls.offset[1]});
        js_offset.Push(Sys::JsNumber{ls.offset[2]});

        js_out.Insert("offset", std::move(js_offset));
    }

    if (ls.radius != 1.0f && ls.type != eLightType::Rect) {
        js_out.Insert("radius", Sys::JsNumber{ls.radius});
    }

    if (ls.width != 1.0f) {
        js_out.Insert("width", Sys::JsNumber{ls.width});
    }

    if (ls.height != 1.0f) {
        js_out.Insert("height", Sys::JsNumber{ls.height});
    }

    if (ls.dir[0] != 0.0f || ls.dir[1] != -1.0f || ls.dir[2] != 0.0f) {
        Sys::JsArrayP js_dir(alloc);

        js_dir.Push(Sys::JsNumber{ls.dir[0]});
        js_dir.Push(Sys::JsNumber{ls.dir[1]});
        js_dir.Push(Sys::JsNumber{ls.dir[2]});

        js_out.Insert("direction", std::move(js_dir));
    }

    const float cull_radius = calc_cull_radius(ls.col, ls.radius);
    if (ls.cull_radius != cull_radius) {
        js_out.Insert("cull_radius", Sys::JsNumber{ls.cull_radius});
    }

    if (ls.angle_deg != default_angle(ls.type)) {
        js_out.Insert("spot_angle", Sys::JsNumber{ls.angle_deg});
    }

    if (ls.spot_blend != 0.0f) {
        js_out.Insert("spot_blend", Sys::JsNumber{ls.spot_blend});
    }

    if (ls.cull_offset != 0.1f) {
        js_out.Insert("cull_offset", Sys::JsNumber{ls.cull_offset});
    }

    if (ls.flags & eLightFlags::SkyPortal) {
        js_out.Insert("sky_portal", Sys::JsLiteral{Sys::JsLiteralType::True});
    }

    if (ls.flags & eLightFlags::CastShadow) {
        js_out.Insert("cast_shadow", Sys::JsLiteral{Sys::JsLiteralType::True});
    }

    if (!(ls.flags & eLightFlags::AffectDiffuse)) {
        js_out.Insert("affect_diffuse", Sys::JsLiteral{Sys::JsLiteralType::False});
    }

    if (!(ls.flags & eLightFlags::AffectSpecular)) {
        js_out.Insert("affect_specular", Sys::JsLiteral{Sys::JsLiteralType::False});
    }

    if (!(ls.flags & eLightFlags::AffectRefraction)) {
        js_out.Insert("affect_refraction", Sys::JsLiteral{Sys::JsLiteralType::False});
    }

    if (!(ls.flags & eLightFlags::AffectVolume)) {
        js_out.Insert("affect_volume", Sys::JsLiteral{Sys::JsLiteralType::False});
    }

    if (ls.shadow_bias[0] != 4.0f || ls.shadow_bias[1] != 8.0f) {
        Sys::JsArrayP js_shadow_bias(alloc);

        js_shadow_bias.Push(Sys::JsNumber{ls.shadow_bias[0]});
        js_shadow_bias.Push(Sys::JsNumber{ls.shadow_bias[1]});

        js_out.Insert("shadow_bias", std::move(js_shadow_bias));
    }
}