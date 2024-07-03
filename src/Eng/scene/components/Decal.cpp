#include "Decal.h"

#include <Sys/Json.h>

void Eng::Decal::Read(const JsObjectP &js_in, Decal &de) {
    if (js_in.Has("pos")) {
        const JsArrayP &js_pos = js_in.at("pos").as_arr();

        const auto pos = Ren::Vec3f{float(js_pos.at(0).as_num().val), float(js_pos.at(1).as_num().val),
                                    float(js_pos.at(2).as_num().val)};

        de.view = Translate(de.view, pos);
    }

    if (js_in.Has("rot")) {
        const JsArrayP &js_rot = js_in.at("rot").as_arr();

        auto rot = Ren::Vec3f{float(js_rot.at(0).as_num().val), float(js_rot.at(1).as_num().val),
                              float(js_rot.at(2).as_num().val)};

        rot *= Ren::Pi<float>() / 180.0f;

        Ren::Mat4f rot_z = Rotate(Ren::Mat4f{1.0f}, rot[2], Ren::Vec3f{0.0f, 0.0f, 1.0f});
        Ren::Mat4f rot_x = Rotate(Ren::Mat4f{1.0f}, rot[0], Ren::Vec3f{1.0f, 0.0f, 0.0f});
        Ren::Mat4f rot_y = Rotate(Ren::Mat4f{1.0f}, rot[1], Ren::Vec3f{0.0f, 1.0f, 0.0f});

        Ren::Mat4f rot_all = rot_y * rot_x * rot_z;
        de.view = de.view * rot_all;
    }

    de.view = Inverse(de.view);

    auto dim = Ren::Vec3f{1.0f, 1.0f, 1.0f};

    if (js_in.Has("dim")) {
        const JsArrayP &js_dim = js_in.at("dim").as_arr();

        dim = Ren::Vec3f{float(js_dim.at(0).as_num().val), float(js_dim.at(1).as_num().val),
                         float(js_dim.at(2).as_num().val)};
    }

    de.proj = Ren::OrthographicProjection(-0.5f * dim[0], 0.5f * dim[0], -0.5f * dim[1], 0.5f * dim[1], 0.0f,
                                          1.0f * dim[2], true /* z_range_zero_to_one */);
}

void Eng::Decal::Write(const Decal &de, JsObjectP &js_out) {}
