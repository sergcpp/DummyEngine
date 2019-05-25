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

        float angle = 45.0f;
        if (js_in.Has("angle")) {
            const auto &js_angle = (const JsNumber &)js_in.at("angle");
            angle = (float)js_angle.val;
        }

        float angle_rad = angle * Ren::Pi<float>() / 180.0f;

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

    if (js_in.Has("cache_shadow")) {
        ls.cache_shadow = ((const JsLiteral &)js_in.at("cache_shadow")).val == JS_TRUE;
    } else {
        ls.cache_shadow = false;
    }
}

void LightSource::Write(const LightSource &ls, JsObject &js_out) {

}