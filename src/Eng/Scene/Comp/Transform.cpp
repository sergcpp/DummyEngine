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
        const JsArray &js_pos = js_in.at("pos").as_arr();

        const auto pos = Ren::Vec3f{
            (float)js_pos.at(0).as_num().val, (float)js_pos.at(1).as_num().val, (float)js_pos.at(2).as_num().val
        };

        tr.mat = Ren::Translate(tr.mat, pos);
    }

    if (js_in.Has("rot")) {
        const JsArray &js_rot = js_in.at("rot").as_arr();

        // angles in degrees
        tr.euler_angles_rad = Ren::Vec3f{
            (float)js_rot.at(0).as_num().val, (float)js_rot.at(1).as_num().val, (float)js_rot.at(2).as_num().val
        };

        // convert to radians
        tr.euler_angles_rad *= Ren::Pi<float>() / 180.0f;

        const Ren::Mat4f
            rot_z = Ren::Rotate(Ren::Mat4f{ 1.0f }, tr.euler_angles_rad[2], Ren::Vec3f{ 0.0f, 0.0f, 1.0f }),
            rot_x = Ren::Rotate(Ren::Mat4f{ 1.0f }, tr.euler_angles_rad[0], Ren::Vec3f{ 1.0f, 0.0f, 0.0f }),
            rot_y = Ren::Rotate(Ren::Mat4f{ 1.0f }, tr.euler_angles_rad[1], Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

        Ren::Mat4f rot_all = rot_y * rot_x * rot_z;
        tr.mat = tr.mat * rot_all;
    }
}

void Transform::Write(const Transform &tr, JsObject &js_out) {
    {   // write position
        JsArray js_pos;

        js_pos.Push(JsNumber((double)tr.mat[3][0]));
        js_pos.Push(JsNumber((double)tr.mat[3][1]));
        js_pos.Push(JsNumber((double)tr.mat[3][2]));

        js_out.Push("pos", std::move(js_pos));
    }

    {   // write rotation
        JsArray js_rot;

        const Ren::Vec3f euler_angles_deg = tr.euler_angles_rad * 180.0f / Ren::Pi<float>();

        js_rot.Push(JsNumber((double)euler_angles_deg[0]));
        js_rot.Push(JsNumber((double)euler_angles_deg[1]));
        js_rot.Push(JsNumber((double)euler_angles_deg[2]));

        js_out.Push("rot", std::move(js_rot));
    }
}

//
// Euler angles from matrix (maybe will need it later)
//
/*
    // https://www.gregslabaugh.net/publications/euler.pdf

    float theta_x, theta_z;
    float theta_y = std::asin(tr.mat[2][0]);
    if (theta_y < Ren::Pi<float>() / 2.0f ) {
        if (theta_y > -Ren::Pi<float>() / 2.0f) {
            theta_x = std::atan2(-tr.mat[2][1], tr.mat[2][2]);
            theta_z = std::atan2(-tr.mat[1][0], tr.mat[0][0]);
        } else {
            // not a unique solution
            theta_x = -std::atan2(tr.mat[0][1], tr.mat[1][1]);
            theta_z = 0.0f;
        }
    } else {
        // not a unique solution
        theta_x = std::atan2(tr.mat[0][1], tr.mat[1][1]);
        theta_z = 0.0f;
    }

    // convert to degrees
    theta_x *= 180.0f / Ren::Pi<float>();
    theta_y *= 180.0f / Ren::Pi<float>();
    theta_z *= 180.0f / Ren::Pi<float>();

*/