#include "SceneData.h"

#include <Ren/MMat.h>
#include <Sys/Json.h>

void Transform::Read(const JsObject &js_in) {
    if (js_in.Has("pos")) {
        const JsArray &js_pos = (const JsArray &)js_in.at("pos");

        Ren::Vec3f pos = { (float)((const JsNumber &)js_pos.at(0)).val,
                           (float)((const JsNumber &)js_pos.at(1)).val,
                           (float)((const JsNumber &)js_pos.at(2)).val };

        mat = Ren::Translate(mat, pos);
    }

    if (js_in.Has("rot")) {
        const JsArray &js_rot = (const JsArray &)js_in.at("rot");

        Ren::Vec3f rot = { (float)((const JsNumber &)js_rot.at(0)).val,
            (float)((const JsNumber &)js_rot.at(1)).val,
            (float)((const JsNumber &)js_rot.at(2)).val };

        rot *= Ren::Pi<float>() / 180.0f;

        auto rot_z = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[2], Ren::Vec3f{ 0.0f, 0.0f, 1.0f });
        auto rot_x = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[0], Ren::Vec3f{ 1.0f, 0.0f, 0.0f });
        auto rot_y = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[1], Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

        auto rot_all = rot_y * rot_x * rot_z;
        mat = mat * rot_all;
    }
}

void Transform::Write(JsObject &js_out) {

}

void LightSource::Read(const JsObject &js_in) {
    const auto &js_color = (const JsArray &)js_in.at("color");

    col[0] = (float)static_cast<const JsNumber &>(js_color[0]).val;
    col[1] = (float)static_cast<const JsNumber &>(js_color[1]).val;
    col[2] = (float)static_cast<const JsNumber &>(js_color[2]).val;

    brightness = std::max(col[0], std::max(col[1], col[2]));

    if (js_in.Has("offset")) {
        const auto &js_offset = (const JsArray &)js_in.at("offset");

        offset[0] = (float)static_cast<const JsNumber &>(js_offset[0]).val;
        offset[1] = (float)static_cast<const JsNumber &>(js_offset[1]).val;
        offset[2] = (float)static_cast<const JsNumber &>(js_offset[2]).val;
    }

    if (js_in.Has("radius")) {
        const auto &js_radius = (const JsNumber &)js_in.at("radius");

        radius = (float)js_radius.val;
    } else {
        radius = 1.0f;
    }

    influence = radius * (std::sqrt(brightness / LIGHT_ATTEN_CUTOFF) - 1.0f);

    if (js_in.Has("direction")) {
        const auto &js_dir = (const JsArray &)js_in.at("direction");

        dir[0] = (float)static_cast<const JsNumber &>(js_dir[0]).val;
        dir[1] = (float)static_cast<const JsNumber &>(js_dir[1]).val;
        dir[2] = (float)static_cast<const JsNumber &>(js_dir[2]).val;

        float angle = 45.0f;
        if (js_in.Has("angle")) {
            const auto &js_angle = (const JsNumber &)js_in.at("angle");
            angle = (float)js_angle.val;
        }

        spot = std::cos(angle * Ren::Pi<float>() / 180.0f);
    } else {
        dir[1] = -1.0f;
        spot = -1.0f;
    }

    if (js_in.Has("cast_shadow")) {
        cast_shadow = ((const JsLiteral &)js_in.at("cast_shadow")).val == JS_TRUE;
    } else {
        cast_shadow = false;
    }

    if (js_in.Has("cache_shadow")) {
        cache_shadow = ((const JsLiteral &)js_in.at("cache_shadow")).val == JS_TRUE;
    } else {
        cache_shadow = false;
    }
}

void LightSource::Write(JsObject &js_out) {

}

//////////////////////////////////////////////////////////////////////////////////////

void Decal::Read(const JsObject &js_in) {
    if (js_in.Has("pos")) {
        const JsArray &js_pos = (const JsArray &)js_in.at("pos");

        Ren::Vec3f pos = { (float)((const JsNumber &)js_pos.at(0)).val,
                           (float)((const JsNumber &)js_pos.at(1)).val,
                           (float)((const JsNumber &)js_pos.at(2)).val };

        view = Ren::Translate(view, pos);
    }

    if (js_in.Has("rot")) {
        const JsArray &js_rot = (const JsArray &)js_in.at("rot");

        Ren::Vec3f rot = { (float)((const JsNumber &)js_rot.at(0)).val,
            (float)((const JsNumber &)js_rot.at(1)).val,
            (float)((const JsNumber &)js_rot.at(2)).val };

        rot *= Ren::Pi<float>() / 180.0f;

        auto rot_z = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[2], Ren::Vec3f{ 0.0f, 0.0f, 1.0f });
        auto rot_x = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[0], Ren::Vec3f{ 1.0f, 0.0f, 0.0f });
        auto rot_y = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[1], Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

        auto rot_all = rot_y * rot_x * rot_z;
        view = view * rot_all;
    }

    view = Ren::Inverse(view);

    Ren::Vec3f dim = { 1.0f, 1.0f, 1.0f };

    if (js_in.Has("dim")) {
        const JsArray &js_dim = (const JsArray &)js_in.at("dim");

        dim = { (float)((const JsNumber &)js_dim.at(0)).val,
                (float)((const JsNumber &)js_dim.at(1)).val,
                (float)((const JsNumber &)js_dim.at(2)).val };
    }

    Ren::OrthographicProjection(proj, -0.5f * dim[0], 0.5f * dim[0], -0.5f * dim[1], 0.5f * dim[1], 0.0f, 1.0f * dim[2]);
}

void Decal::Write(JsObject &js_out) {

}