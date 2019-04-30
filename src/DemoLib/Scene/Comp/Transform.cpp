#include "Transform.h"

#include <Sys/Json.h>

void Transform::UpdateBBox() {
    bbox_min_ws = bbox_max_ws = Ren::Vec3f(mat[3]);

    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            float a = mat[i][j] * bbox_min[i];
            float b = mat[i][j] * bbox_max[i];

            if (a < b) {
                bbox_min_ws[j] += a;
                bbox_max_ws[j] += b;
            } else {
                bbox_min_ws[j] += b;
                bbox_max_ws[j] += a;
            }
        }
    }
}

void Transform::Read(const JsObject &js_in, Transform &tr) {
    if (js_in.Has("pos")) {
        const JsArray &js_pos = (const JsArray &)js_in.at("pos");

        Ren::Vec3f pos = { (float)((const JsNumber &)js_pos.at(0)).val,
                           (float)((const JsNumber &)js_pos.at(1)).val,
                           (float)((const JsNumber &)js_pos.at(2)).val };

        tr.mat = Ren::Translate(tr.mat, pos);
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
        tr.mat = tr.mat * rot_all;
    }
}

void Transform::Write(const Transform &tr, JsObject &js_out) {

}