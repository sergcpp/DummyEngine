#include "Transform.h"

#include <Sys/Json.h>

void Eng::Transform::UpdateBBox() {
    bbox_min_ws = bbox_max_ws = Ren::Vec3f(world_from_object[3]);

    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            const float a = world_from_object[i][j] * bbox_min[i];
            const float b = world_from_object[i][j] * bbox_max[i];

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

void Eng::Transform::UpdateInvMatrix() { object_from_world = InverseAffine(world_from_object); }

void Eng::Transform::Read(const Sys::JsObjectP &js_in, Transform &tr) {
    tr.world_from_object = Ren::Mat4f{1.0f};

    if (const size_t pos_ndx = js_in.IndexOf("pos"); pos_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_pos = js_in[pos_ndx].second.as_arr();

        tr.position[0] = js_pos.at(0).as_num().val;
        tr.position[1] = js_pos.at(1).as_num().val;
        tr.position[2] = js_pos.at(2).as_num().val;

        tr.world_from_object = Translate(tr.world_from_object, Ren::Vec3f(tr.position));
    }

    if (const size_t rot_ndx = js_in.IndexOf("rot"); rot_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_rot = js_in[rot_ndx].second.as_arr();

        // angles in degrees
        tr.euler_angles_rad = Ren::Vec3f{float(js_rot.at(0).as_num().val), float(js_rot.at(1).as_num().val),
                                         float(js_rot.at(2).as_num().val)};

        // convert to radians
        tr.euler_angles_rad *= Ren::Pi<float>() / 180.0f;

        static const Ren::Vec3f axes[] = {Ren::Vec3f{1.0f, 0.0f, 0.0f}, Ren::Vec3f{0.0f, 1.0f, 0.0f},
                                          Ren::Vec3f{0.0f, 0.0f, 1.0f}};

        for (const int i : {2, 0, 1}) {
            tr.world_from_object = Rotate(tr.world_from_object, tr.euler_angles_rad[i], axes[i]);
        }
    }

    if (const size_t scale_ndx = js_in.IndexOf("scale"); scale_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_scale = js_in[scale_ndx].second.as_arr();

        tr.scale[0] = float(js_scale[0].as_num().val);
        tr.scale[1] = float(js_scale[1].as_num().val);
        tr.scale[2] = float(js_scale[2].as_num().val);

        tr.world_from_object = Scale(tr.world_from_object, tr.scale);
    } else {
        tr.scale = Ren::Vec3f{1.0f, 1.0f, 1.0f};
    }

    tr.UpdateInvMatrix();
}

void Eng::Transform::Write(const Transform &tr, Sys::JsObjectP &js_out) {
    const auto &alloc = js_out.elements.get_allocator();

    { // write position
        Sys::JsArrayP js_pos(alloc);

        js_pos.Push(Sys::JsNumber{tr.position[0]});
        js_pos.Push(Sys::JsNumber{tr.position[1]});
        js_pos.Push(Sys::JsNumber{tr.position[2]});

        js_out.Insert("pos", std::move(js_pos));
    }

    { // write rotation
        Sys::JsArrayP js_rot(alloc);

        const Ren::Vec3f euler_angles_deg = tr.euler_angles_rad * 180.0f / Ren::Pi<float>();

        js_rot.Push(Sys::JsNumber{euler_angles_deg[0]});
        js_rot.Push(Sys::JsNumber{euler_angles_deg[1]});
        js_rot.Push(Sys::JsNumber{euler_angles_deg[2]});

        js_out.Insert("rot", std::move(js_rot));
    }

    if (tr.scale[0] != 1.0f || tr.scale[1] != 1.0f || tr.scale[2] != 1.0f) {
        Sys::JsArrayP js_scale(alloc);

        js_scale.Push(Sys::JsNumber{tr.scale[0]});
        js_scale.Push(Sys::JsNumber{tr.scale[1]});
        js_scale.Push(Sys::JsNumber{tr.scale[2]});

        js_out.Insert("scale", std::move(js_scale));
    }
}

//
// Euler angles from matrix (maybe will be needed later)
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