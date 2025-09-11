#include "Decal.h"

#include <Sys/Json.h>

void Eng::Decal::Read(const Sys::JsObjectP &js_in, Decal &de) {
    if (const size_t pos_ndx = js_in.IndexOf("pos"); pos_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_pos = js_in[pos_ndx].second.as_arr();

        const auto pos = Ren::Vec3f{float(js_pos.at(0).as_num().val), float(js_pos.at(1).as_num().val),
                                    float(js_pos.at(2).as_num().val)};

        de.view = Translate(de.view, pos);
    }

    if (const size_t rot_ndx = js_in.IndexOf("rot"); rot_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_rot = js_in[rot_ndx].second.as_arr();

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

    if (const size_t dim_ndx = js_in.IndexOf("dim"); dim_ndx < js_in.Size()) {
        const Sys::JsArrayP &js_dim = js_in[dim_ndx].second.as_arr();

        dim = Ren::Vec3f{float(js_dim.at(0).as_num().val), float(js_dim.at(1).as_num().val),
                         float(js_dim.at(2).as_num().val)};
    }

    de.proj = Ren::OrthographicProjection(-0.5f * dim[0], 0.5f * dim[0], -0.5f * dim[1], 0.5f * dim[1], 0.0f,
                                          1.0f * dim[2], true /* z_range_zero_to_one */);
}

void Eng::Decal::Write(const Decal &de, Sys::JsObjectP &js_out) {}
