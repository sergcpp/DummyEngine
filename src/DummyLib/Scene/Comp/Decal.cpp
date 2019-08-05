#include "Decal.h"

#include <Sys/Json.h>

void Decal::Read(const JsObject &js_in, Decal &de) {
    if (js_in.Has("pos")) {
        const JsArray &js_pos = (const JsArray &)js_in.at("pos");

        Ren::Vec3f pos = { (float)((const JsNumber &)js_pos.at(0)).val,
                           (float)((const JsNumber &)js_pos.at(1)).val,
                           (float)((const JsNumber &)js_pos.at(2)).val };

        de.view = Ren::Translate(de.view, pos);
    }

    if (js_in.Has("rot")) {
        const JsArray &js_rot = (const JsArray &)js_in.at("rot");

        Ren::Vec3f rot = { (float)((const JsNumber &)js_rot.at(0)).val,
                           (float)((const JsNumber &)js_rot.at(1)).val,
                           (float)((const JsNumber &)js_rot.at(2)).val };

        rot *= Ren::Pi<float>() / 180.0f;

        Ren::Mat4f rot_z = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[2], Ren::Vec3f{ 0.0f, 0.0f, 1.0f });
        Ren::Mat4f rot_x = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[0], Ren::Vec3f{ 1.0f, 0.0f, 0.0f });
        Ren::Mat4f rot_y = Ren::Rotate(Ren::Mat4f{ 1.0f }, rot[1], Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

        Ren::Mat4f rot_all = rot_y * rot_x * rot_z;
        de.view = de.view * rot_all;
    }

    de.view = Ren::Inverse(de.view);

    Ren::Vec3f dim = { 1.0f, 1.0f, 1.0f };

    if (js_in.Has("dim")) {
        const JsArray &js_dim = (const JsArray &)js_in.at("dim");

        dim = { (float)((const JsNumber &)js_dim.at(0)).val,
                (float)((const JsNumber &)js_dim.at(1)).val,
                (float)((const JsNumber &)js_dim.at(2)).val };
    }

    Ren::OrthographicProjection(de.proj, -0.5f * dim[0], 0.5f * dim[0], -0.5f * dim[1], 0.5f * dim[1], 0.0f, 1.0f * dim[2]);
}

void Decal::Write(const Decal &de, JsObject &js_out) {

}