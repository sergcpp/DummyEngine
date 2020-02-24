#include "LightSource.h"

#include <algorithm>

#include <Sys/Json.h>

void LightSource::Read(const JsObject &js_in, LightSource &ls) {
    const JsArray &js_color = js_in.at("color").as_arr();

    ls.col[0] = float(js_color[0].as_num().val);
    ls.col[1] = float(js_color[1].as_num().val);
    ls.col[2] = float(js_color[2].as_num().val);

    ls.brightness = std::max(ls.col[0], std::max(ls.col[1], ls.col[2]));

    if (js_in.Has("offset")) {
        const JsArray &js_offset = js_in.at("offset").as_arr();

        ls.offset[0] = float(js_offset[0].as_num().val);
        ls.offset[1] = float(js_offset[1].as_num().val);
        ls.offset[2] = float(js_offset[2].as_num().val);
    }

    if (js_in.Has("radius")) {
        const JsNumber &js_radius = js_in.at("radius").as_num();
        ls.radius = float(js_radius.val);
    } else {
        ls.radius = 1.0f;
    }

    ls.influence = ls.radius * (std::sqrt(ls.brightness / LIGHT_ATTEN_CUTOFF) - 1.0f);

    if (js_in.Has("direction")) {
        const JsArray &js_dir = js_in.at("direction").as_arr();

        ls.dir[0] = float(js_dir[0].as_num().val);
        ls.dir[1] = float(js_dir[1].as_num().val);
        ls.dir[2] = float(js_dir[2].as_num().val);

        ls.angle_deg = 45.0f;
        if (js_in.Has("angle")) {
            const JsNumber &js_angle = js_in.at("angle").as_num();
            ls.angle_deg = float(js_angle.val);
        }

        const float angle_rad = ls.angle_deg * Ren::Pi<float>() / 180.0f;

        ls.spot = std::cos(angle_rad);
        ls.cap_radius = ls.influence * std::tan(angle_rad);
    } else {
        ls.dir[1] = -1.0f;
        ls.spot = -1.2f;
    }

    if (js_in.Has("cast_shadow")) {
        ls.cast_shadow = js_in.at("cast_shadow").as_lit().val == JsLiteralType::True;
    } else {
        ls.cast_shadow = false;
    }

    if (js_in.Has("shadow_bias")) {
        const JsArray &js_shadow_bias = js_in.at("shadow_bias").as_arr();
        ls.shadow_bias[0] = float(js_shadow_bias.at(0).as_num().val);
        ls.shadow_bias[1] = float(js_shadow_bias.at(1).as_num().val);
    } else {
        ls.shadow_bias[0] = 4.0f;
        ls.shadow_bias[1] = 8.0f;
    }
}

void LightSource::Write(const LightSource &ls, JsObject &js_out) {
    { // Write color
        JsArray js_color;

        js_color.Push(JsNumber{double(ls.col[0])});
        js_color.Push(JsNumber{double(ls.col[1])});
        js_color.Push(JsNumber{double(ls.col[2])});

        js_out.Push("color", std::move(js_color));
    }

    { // Write offset
        JsArray js_offset;

        js_offset.Push(JsNumber{double(ls.offset[0])});
        js_offset.Push(JsNumber{double(ls.offset[1])});
        js_offset.Push(JsNumber{double(ls.offset[2])});

        js_out.Push("offset", std::move(js_offset));
    }

    if (ls.radius != 1.0f) {
        js_out.Push("radius", JsNumber{double(ls.radius)});
    }

    { // Write direction and angle
        JsArray js_dir;

        js_dir.Push(JsNumber{double(ls.dir[0])});
        js_dir.Push(JsNumber{double(ls.dir[1])});
        js_dir.Push(JsNumber{double(ls.dir[2])});

        js_out.Push("direction", std::move(js_dir));

        if (ls.angle_deg != 45.0f) {
            js_out.Push("angle", JsNumber{double(ls.angle_deg)});
        }
    }

    if (ls.cast_shadow) {
        js_out.Push("cast_shadow", JsLiteral{JsLiteralType::True});
    }

    if (ls.shadow_bias[0] != 4.0f || ls.shadow_bias[1] != 8.0f) {
        JsArray js_shadow_bias;

        js_shadow_bias.Push(JsNumber{double(ls.shadow_bias[0])});
        js_shadow_bias.Push(JsNumber{double(ls.shadow_bias[1])});

        js_out.Push("shadow_bias", std::move(js_shadow_bias));
    }
}