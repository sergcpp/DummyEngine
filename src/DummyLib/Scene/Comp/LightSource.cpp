#include "LightSource.h"

#include <algorithm>

#include <Sys/Json.h>

void LightSource::Read(const JsObject &js_in, LightSource &ls) {
    const auto &js_color = (const JsArray &)js_in.at("color");

    ls.col[0] = (float)static_cast<const JsNumber &>(js_color[0]).val;
    ls.col[1] = (float)static_cast<const JsNumber &>(js_color[1]).val;
    ls.col[2] = (float)static_cast<const JsNumber &>(js_color[2]).val;

    ls.brightness = std::max(ls.col[0], std::max(ls.col[1], ls.col[2]));

    if (js_in.Has("offset")) {
        const auto &js_offset = (const JsArray &)js_in.at("offset");

        ls.offset[0] = (float)static_cast<const JsNumber &>(js_offset[0]).val;
        ls.offset[1] = (float)static_cast<const JsNumber &>(js_offset[1]).val;
        ls.offset[2] = (float)static_cast<const JsNumber &>(js_offset[2]).val;
    }

    if (js_in.Has("radius")) {
        const auto &js_radius = (const JsNumber &)js_in.at("radius");

        ls.radius = (float)js_radius.val;
    } else {
        ls.radius = 1.0f;
    }

    ls.influence = ls.radius * (std::sqrt(ls.brightness / LIGHT_ATTEN_CUTOFF) - 1.0f);

    if (js_in.Has("direction")) {
        const auto &js_dir = (const JsArray &)js_in.at("direction");

        ls.dir[0] = (float)static_cast<const JsNumber &>(js_dir[0]).val;
        ls.dir[1] = (float)static_cast<const JsNumber &>(js_dir[1]).val;
        ls.dir[2] = (float)static_cast<const JsNumber &>(js_dir[2]).val;

        ls.angle_deg = 45.0f;
        if (js_in.Has("angle")) {
            const auto &js_angle = (const JsNumber &)js_in.at("angle");
            ls.angle_deg = (float)js_angle.val;
        }

        const float angle_rad = ls.angle_deg * Ren::Pi<float>() / 180.0f;

        ls.spot = std::cos(angle_rad);
        ls.cap_radius = ls.influence * std::tan(angle_rad);
    } else {
        ls.dir[1] = -1.0f;
        ls.spot = -1.2f;
    }

    if (js_in.Has("cast_shadow")) {
        ls.cast_shadow = ((const JsLiteral &)js_in.at("cast_shadow")).val == JS_TRUE;
    } else {
        ls.cast_shadow = false;
    }
}

void LightSource::Write(const LightSource &ls, JsObject &js_out) {
    {   // Write color
        JsArray js_color;

        js_color.Push(JsNumber{ (double)ls.col[0] });
        js_color.Push(JsNumber{ (double)ls.col[1] });
        js_color.Push(JsNumber{ (double)ls.col[2] });

        js_out.Push("color", std::move(js_color));
    }

    {   // Write offset
        JsArray js_offset;

        js_offset.Push(JsNumber{ (double)ls.offset[0] });
        js_offset.Push(JsNumber{ (double)ls.offset[1] });
        js_offset.Push(JsNumber{ (double)ls.offset[2] });

        js_out.Push("offset", std::move(js_offset));
    }

    if (ls.radius != 1.0f) {
        js_out.Push("radius", JsNumber{ (double)ls.radius });
    }

    {   // Write direction and angle
        JsArray js_dir;

        js_dir.Push(JsNumber{ (double)ls.dir[0] });
        js_dir.Push(JsNumber{ (double)ls.dir[1] });
        js_dir.Push(JsNumber{ (double)ls.dir[2] });

        js_out.Push("direction", std::move(js_dir));

        if (ls.angle_deg != 45.0f) {
            js_out.Push("angle", JsNumber{ (double)ls.angle_deg });
        }
    }

    if (ls.cast_shadow) {
        js_out.Push("cast_shadow", JsLiteral{ JS_TRUE });
    }
}