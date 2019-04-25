#include "Transform.h"

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