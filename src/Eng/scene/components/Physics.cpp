#include "Physics.h"

#include <Sys/Json.h>

void Eng::Physics::Read(const Sys::JsObjectP &js_in, Physics &ph) {
    using Phy::real;

    if (js_in.Has("pos")) {
        const Sys::JsArrayP &js_pos = js_in.at("pos").as_arr();

        ph.body.pos =
            Phy::Vec3(real(js_pos[0].as_num().val), real(js_pos[1].as_num().val), real(js_pos[2].as_num().val));
    } else {
        ph.body.pos = Phy::Vec3{};
    }

    if (js_in.Has("vel")) {
        const Sys::JsArrayP &js_vel = js_in.at("vel").as_arr();

        ph.body.vel_lin =
            Phy::Vec3(real(js_vel[0].as_num().val), real(js_vel[1].as_num().val), real(js_vel[2].as_num().val));
    } else {
        ph.body.vel_lin = Phy::Vec3{};
    }

    ph.body.vel_ang = {};

    if (js_in.Has("rot")) {
        const Sys::JsArrayP &js_rot = js_in.at("rot").as_arr();

        if (js_rot.Size() == 4) { // quaternion
            ph.body.rot = Phy::Quat(real(js_rot[0].as_num().val), real(js_rot[1].as_num().val),
                                    real(js_rot[2].as_num().val), real(js_rot[3].as_num().val));
        } else if (js_rot.Size() == 3) { // euler angles
            const real ToRadians = Phy::Pi<real>() / real(180);
            ph.body.rot =
                Phy::ToQuat(real(js_rot[2].as_num().val) * ToRadians, real(js_rot[1].as_num().val) * ToRadians,
                            real(js_rot[0].as_num().val) * ToRadians);
        }
    } else {
        ph.body.rot = Phy::Quat{};
    }

    if (js_in.Has("inv_mass")) {
        const Sys::JsNumber &js_inv_mass = js_in.at("inv_mass").as_num();
        ph.body.inv_mass = real(js_inv_mass.val);
    } else {
        ph.body.inv_mass = real(0);
    }

    if (js_in.Has("elasticity")) {
        const Sys::JsNumber &js_elasticity = js_in.at("elasticity").as_num();
        ph.body.elasticity = real(js_elasticity.val);
    } else {
        ph.body.elasticity = real(1);
    }

    if (js_in.Has("friction")) {
        const Sys::JsNumber &js_friction = js_in.at("friction").as_num();
        ph.body.friction = real(js_friction.val);
    } else {
        ph.body.friction = real(0);
    }

    {
        const Sys::JsObjectP &js_shape = js_in.at("shape").as_obj();
        const Sys::JsStringP &js_shape_type = js_shape.at("type").as_str();
        if (js_shape_type.val == "sphere") {
            const Sys::JsNumber &js_radius = js_shape.at("radius").as_num();
            ph.body.shape = std::make_unique<Phy::ShapeSphere>(real(js_radius.val));
        } else if (js_shape_type.val == "box") {
            const Sys::JsArrayP &js_points = js_shape.at("points").as_arr();

            std::unique_ptr<Phy::Vec3[]> points(new Phy::Vec3[js_points.Size()]);
            for (size_t i = 0; i < js_points.Size(); i++) {
                const Sys::JsArrayP &js_point = js_points[i].as_arr();
                points[i] = Phy::Vec3{real(js_point[0].as_num().val), real(js_point[1].as_num().val),
                                      real(js_point[2].as_num().val)};
            }

            ph.body.shape = std::make_unique<Phy::ShapeBox>(points.get(), int(js_points.Size()));
        } else if (js_shape_type.val == "convex_hull") {
        }
    }
}

void Eng::Physics::Write(const Physics &ph, Sys::JsObjectP &js_out) {}