#include "GSPhyTest.h"

#include <memory>

#include <Eng/Gui/Renderer.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Eng/Utils/ShaderLoader.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Viewer.h"

namespace GSPhyTestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "phy_test.json";
} // namespace GSPhyTestInternal

GSPhyTest::GSPhyTest(Eng::GameBase *game) : GSBaseState(game) {}

void GSPhyTest::Enter() {
    using namespace GSPhyTestInternal;

    GSBaseState::Enter();

    log_->Info("GSPhyTest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);
}

void GSPhyTest::OnPreloadScene(JsObjectP &js_scene) {
    GSBaseState::OnPreloadScene(js_scene);

    JsArrayP &js_objects = js_scene.at("objects").as_arr();
    const auto &alloc = js_scene.elements.get_allocator();

    // Dynamic spheres
    for (int x = 0; x < 0; x++) {
        for (int z = 0; z < 6; z++) {
            const double Radius = 0.5;
            const double xx = double(x - 1LL) * Radius * 1.5;
            const double zz = double(z - 1LL) * Radius * 1.5;

            JsObjectP new_object(alloc);

            { // Add empty transform component
                JsObjectP js_transform(alloc);
                { // scale unit sphere to radius
                    JsArrayP js_scale(alloc);
                    js_scale.Push(JsNumber{Radius});
                    js_scale.Push(JsNumber{Radius});
                    js_scale.Push(JsNumber{Radius});
                    js_transform.Push("scale", std::move(js_scale));
                }
                new_object.Push(Eng::Transform::name(), std::move(js_transform));
            }

            { // Add physics component
                JsObjectP js_physics(alloc);

                { // position
                    JsArrayP js_pos(alloc);
                    js_pos.Push(JsNumber{xx});
                    js_pos.Push(JsNumber{10.0});
                    js_pos.Push(JsNumber{zz});
                    js_physics.Push("pos", std::move(js_pos));
                }

                js_physics.Push("inv_mass", JsNumber{1.0});
                js_physics.Push("elasticity", JsNumber{0.0});
                js_physics.Push("friction", JsNumber{0.5});

                { // shape
                    JsObjectP js_shape(alloc);
                    js_shape.Push("type", JsStringP{"sphere", alloc});
                    js_shape.Push("radius", JsNumber{Radius});
                    js_physics.Push("shape", std::move(js_shape));
                }

                new_object.Push(Eng::Physics::name(), std::move(js_physics));
            }

            { // Add drawable component
                JsObjectP js_drawable(alloc);
                js_drawable.Push("mesh_file", JsStringP{"sphere_hi.mesh", alloc});
                js_drawable.Push("visible_to_probes", JsLiteral{JsLiteralType::False});
                new_object.Push(Eng::Drawable::name(), std::move(js_drawable));
            }

            js_objects.Push(std::move(new_object));
        }
    }

    // Static spheres
    for (int x = 0; x < 1; x++) {
        for (int z = 0; z < 1; z++) {
            const double Radius = 80;
            const double xx = double(x - 1LL) * Radius * 0.25;
            const double zz = double(z - 1LL) * Radius * 0.25;

            JsObjectP new_object(alloc);

            { // Add empty transform component
                JsObjectP js_transform(alloc);
                { // scale unit sphere to radius
                    JsArrayP js_scale(alloc);
                    js_scale.Push(JsNumber{Radius});
                    js_scale.Push(JsNumber{Radius});
                    js_scale.Push(JsNumber{Radius});
                    js_transform.Push("scale", std::move(js_scale));
                }
                new_object.Push(Eng::Transform::name(), std::move(js_transform));
            }

            { // Add physics component
                JsObjectP js_physics(alloc);

                { // position
                    JsArrayP js_pos(alloc);
                    js_pos.Push(JsNumber{xx});
                    js_pos.Push(JsNumber{-Radius});
                    js_pos.Push(JsNumber{zz});
                    js_physics.Push("pos", std::move(js_pos));
                }

                js_physics.Push("inv_mass", JsNumber{0.0});
                js_physics.Push("elasticity", JsNumber{0.99});
                js_physics.Push("friction", JsNumber{0.5});

                { // shape
                    JsObjectP js_shape(alloc);
                    js_shape.Push("type", JsStringP{"sphere", alloc});
                    js_shape.Push("radius", JsNumber{Radius});
                    js_physics.Push("shape", std::move(js_shape));
                }

                new_object.Push(Eng::Physics::name(), std::move(js_physics));
            }

            { // Add drawable component
                JsObjectP js_drawable(alloc);
                js_drawable.Push("mesh_file", JsStringP{"sphere_hi.mesh", alloc});
                js_drawable.Push("visible_to_probes", JsLiteral{JsLiteralType::False});

                { // Set material to checkerboard
                    JsArrayP js_material_override(alloc);
                    js_material_override.Push(JsStringP{"checker.txt", alloc});
                    js_drawable.Push("material_override", std::move(js_material_override));
                }

                new_object.Push(Eng::Drawable::name(), std::move(js_drawable));
            }

            js_objects.Push(std::move(new_object));
        }
    }
}

void GSPhyTest::OnPostloadScene(JsObjectP &js_scene) {
    GSBaseState::OnPostloadScene(js_scene);

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            initial_view_pos_[0] = float(js_orig.at(0).as_num().val);
            initial_view_pos_[1] = float(js_orig.at(1).as_num().val);
            initial_view_pos_[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            initial_view_dir_[0] = float(js_dir.at(0).as_num().val);
            initial_view_dir_[1] = float(js_dir.at(1).as_num().val);
            initial_view_dir_[2] = float(js_dir.at(2).as_num().val);
        }

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            max_fwd_speed_ = float(js_fwd_speed.val);
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov_ = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure_ = float(js_max_exposure.val);
        }
    }

    view_origin_ = initial_view_pos_;
    view_dir_ = initial_view_dir_;
}

void GSPhyTest::Exit() { GSBaseState::Exit(); }

void GSPhyTest::Draw() { GSBaseState::Draw(); }

void GSPhyTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) { GSBaseState::DrawUI(r, root); }

void GSPhyTest::UpdateFixed(const uint64_t dt_us) {
    using namespace GSPhyTestInternal;

    GSBaseState::UpdateFixed(dt_us);

    const Ren::Vec3f up = Ren::Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir_, up));

    {
        const float fwd_speed =
                        std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
                    side_speed =
                        std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        view_origin_ += view_dir_ * fwd_speed;
        view_origin_ += side * side_speed;

        if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
            invalidate_view_ = true;
        }
    }
}

bool GSPhyTest::HandleInput(const Eng::InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSPhyTestInternal;

    // pt switch for touch controls
    if (evt.type == Eng::RawInputEv::P1Down || evt.type == Eng::RawInputEv::P2Down) {
        if (evt.point.x > float(ren_ctx_->w()) * 0.9f && evt.point.y < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }

                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    bool input_processed = true;

    switch (evt.type) {
    case Eng::RawInputEv::P1Down:
        if (evt.point.x < (float(ren_ctx_->w()) / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case Eng::RawInputEv::P2Down:
        if (evt.point.x < (float(ren_ctx_->w()) / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case Eng::RawInputEv::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case Eng::RawInputEv::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case Eng::RawInputEv::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move.dx, up);
            rot = Rotate(rot, -0.005f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::RawInputEv::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move.dx, up);
            rot = Rotate(rot, 0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::RawInputEv::KeyDown: {
        if (evt.key_code == Eng::KeyUp || (evt.key_code == Eng::KeyW && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyDown ||
                   (evt.key_code == Eng::KeyS && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyLeft ||
                   (evt.key_code == Eng::KeyA && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::KeyRight ||
                   (evt.key_code == Eng::KeyD && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::KeySpace) {
        } else if (evt.key_code == Eng::KeyLeftShift || evt.key_code == Eng::KeyRightShift) {
            shift_down_ = true;
        } else {
            input_processed = false;
        }
    } break;
    case Eng::RawInputEv::KeyUp: {
        if (!cmdline_enabled_ || view_pointer_) {
            if (evt.key_code == Eng::KeyUp || evt.key_code == Eng::KeyW || evt.key_code == Eng::KeyDown ||
                evt.key_code == Eng::KeyS) {
                fwd_press_speed_ = 0;
            } else if (evt.key_code == Eng::KeyLeft || evt.key_code == Eng::KeyA || evt.key_code == Eng::KeyRight ||
                       evt.key_code == Eng::KeyD) {
                side_press_speed_ = 0;
            } else {
                input_processed = false;
            }
        } else {
            input_processed = false;
        }
    } break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}

void GSPhyTest::UpdateAnim(const uint64_t dt_us) {
    const float delta_time_s = dt_us * 0.000001f;

    // Update camera
    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov_, true,
                              max_exposure_);

    // log_->Info("%f %f %f | %f %f %f",
    //        view_origin_[0], view_origin_[1], view_origin_[2],
    //        view_dir_[0], view_dir_[1], view_dir_[2]);
}

void GSPhyTest::SaveScene(JsObjectP &js_scene) {
    GSBaseState::SaveScene(js_scene);

    const auto &alloc = js_scene.elements.get_allocator();

    { // write camera
        JsObjectP js_camera(alloc);

        { // write view origin
            JsArrayP js_view_origin(alloc);
            js_view_origin.Push(JsNumber{initial_view_pos_[0]});
            js_view_origin.Push(JsNumber{initial_view_pos_[1]});
            js_view_origin.Push(JsNumber{initial_view_pos_[2]});

            js_camera.Push("view_origin", std::move(js_view_origin));
        }

        { // write view direction
            JsArrayP js_view_dir(alloc);
            js_view_dir.Push(JsNumber{initial_view_dir_[0]});
            js_view_dir.Push(JsNumber{initial_view_dir_[1]});
            js_view_dir.Push(JsNumber{initial_view_dir_[2]});

            js_camera.Push("view_dir", std::move(js_view_dir));
        }

        { // write forward speed
            js_camera.Push("fwd_speed", JsNumber{max_fwd_speed_});
        }

        { // write fov
            js_camera.Push("fov", JsNumber{view_fov_});
        }

        { // write max exposure
            js_camera.Push("max_exposure", JsNumber{max_exposure_});
        }

        js_scene.Push("camera", std::move(js_camera));
    }
}
