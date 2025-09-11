#include "PhyTest.h"

#include <memory>

#include <Eng/Log.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../widgets/FontStorage.h"

namespace PhyTestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "phy_test.json";
} // namespace PhyTestInternal

PhyTest::PhyTest(Viewer *viewer) : BaseState(viewer) {}

void PhyTest::Enter() {
    using namespace PhyTestInternal;

    BaseState::Enter();

    log_->Info("PhyTest: Loading scene!");
    BaseState::LoadScene(SCENE_NAME);
}

void PhyTest::OnPreloadScene(Sys::JsObjectP &js_scene) {
    BaseState::OnPreloadScene(js_scene);

    Sys::JsArrayP &js_objects = js_scene.at("objects").as_arr();
    const auto &alloc = js_scene.elements.get_allocator();

    // Dynamic spheres
    /*for (int x = 0; x < 0; x++) {
        for (int z = 0; z < 6; z++) {
            const double Radius = 0.5;
            const double xx = double(x - 1LL) * Radius * 1.5;
            const double zz = double(z - 1LL) * Radius * 1.5;

            Sys::JsObjectP new_object(alloc);

            { // Add empty transform component
                Sys::JsObjectP js_transform(alloc);
                { // scale unit sphere to radius
                    Sys::JsArrayP js_scale(alloc);
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_transform.Insert("scale", std::move(js_scale));
                }
                new_object.Insert(Eng::Transform::name(), std::move(js_transform));
            }

            { // Add physics component
                Sys::JsObjectP js_physics(alloc);

                { // position
                    Sys::JsArrayP js_pos(alloc);
                    js_pos.Push(Sys::JsNumber{xx});
                    js_pos.Push(Sys::JsNumber{10.0});
                    js_pos.Push(Sys::JsNumber{zz});
                    js_physics.Insert("pos", std::move(js_pos));
                }

                js_physics.Insert("inv_mass", Sys::JsNumber{1.0});
                js_physics.Insert("elasticity", Sys::JsNumber{0.0});
                js_physics.Insert("friction", Sys::JsNumber{0.5});

                { // shape
                    Sys::JsObjectP js_shape(alloc);
                    js_shape.Insert("type", Sys::JsStringP{"sphere", alloc});
                    js_shape.Insert("radius", Sys::JsNumber{Radius});
                    js_physics.Insert("shape", std::move(js_shape));
                }

                new_object.Insert(Eng::Physics::name(), std::move(js_physics));
            }

            { // Add drawable component
                Sys::JsObjectP js_drawable(alloc);
                js_drawable.Insert("mesh_file", Sys::JsStringP{"sphere_hi.mesh", alloc});
                js_drawable.Insert("visible_to_probes", Sys::JsLiteral{Sys::JsLiteralType::False});
                new_object.Insert(Eng::Drawable::name(), std::move(js_drawable));
            }

            js_objects.Push(std::move(new_object));
        }
    }*/

    // Static spheres
    for (int x = 0; x < 1; x++) {
        for (int z = 0; z < 1; z++) {
            const double Radius = 80;
            const double xx = double(x - 1LL) * Radius * 0.25;
            const double zz = double(z - 1LL) * Radius * 0.25;

            Sys::JsObjectP new_object(alloc);

            { // Add empty transform component
                Sys::JsObjectP js_transform(alloc);
                { // scale unit sphere to radius
                    Sys::JsArrayP js_scale(alloc);
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_scale.Push(Sys::JsNumber{Radius});
                    js_transform.Insert("scale", std::move(js_scale));
                }
                new_object.Insert(Eng::Transform::name(), std::move(js_transform));
            }

            { // Add physics component
                Sys::JsObjectP js_physics(alloc);

                { // position
                    Sys::JsArrayP js_pos(alloc);
                    js_pos.Push(Sys::JsNumber{xx});
                    js_pos.Push(Sys::JsNumber{-Radius});
                    js_pos.Push(Sys::JsNumber{zz});
                    js_physics.Insert("pos", std::move(js_pos));
                }

                js_physics.Insert("inv_mass", Sys::JsNumber{0.0});
                js_physics.Insert("elasticity", Sys::JsNumber{0.99});
                js_physics.Insert("friction", Sys::JsNumber{0.5});

                { // shape
                    Sys::JsObjectP js_shape(alloc);
                    js_shape.Insert("type", Sys::JsStringP{"sphere", alloc});
                    js_shape.Insert("radius", Sys::JsNumber{Radius});
                    js_physics.Insert("shape", std::move(js_shape));
                }

                new_object.Insert(Eng::Physics::name(), std::move(js_physics));
            }

            { // Add drawable component
                Sys::JsObjectP js_drawable(alloc);
                js_drawable.Insert("mesh_file", Sys::JsStringP{"sphere_hi.mesh", alloc});
                js_drawable.Insert("visible_to_probes", Sys::JsLiteral{Sys::JsLiteralType::False});

                { // Set material to checkerboard
                    Sys::JsArrayP js_material_override(alloc);
                    js_material_override.Push(Sys::JsStringP{"checker.mat", alloc});
                    js_drawable.Insert("material_override", std::move(js_material_override));
                }

                new_object.Insert(Eng::Drawable::name(), std::move(js_drawable));
            }

            js_objects.Push(std::move(new_object));
        }
    }
}

void PhyTest::OnPostloadScene(Sys::JsObjectP &js_scene) {
    BaseState::OnPostloadScene(js_scene);

    if (const size_t camera_ndx = js_scene.IndexOf("camera"); camera_ndx < js_scene.Size()) {
        const Sys::JsObjectP &js_cam = js_scene[camera_ndx].second.as_obj();
        if (const size_t view_origin_ndx = js_cam.IndexOf("view_origin"); view_origin_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_orig = js_cam[view_origin_ndx].second.as_arr();
            initial_view_pos_[0] = float(js_orig.at(0).as_num().val);
            initial_view_pos_[1] = float(js_orig.at(1).as_num().val);
            initial_view_pos_[2] = float(js_orig.at(2).as_num().val);
        }
        if (const size_t view_dir_ndx = js_cam.IndexOf("view_dir"); view_dir_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_dir = js_cam[view_dir_ndx].second.as_arr();
            initial_view_dir_[0] = float(js_dir.at(0).as_num().val);
            initial_view_dir_[1] = float(js_dir.at(1).as_num().val);
            initial_view_dir_[2] = float(js_dir.at(2).as_num().val);
        }
        if (const size_t fwd_speed_ndx = js_cam.IndexOf("fwd_speed"); fwd_speed_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fwd_speed = js_cam[fwd_speed_ndx].second.as_num();
            max_fwd_speed_ = float(js_fwd_speed.val);
        }
        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            view_fov_ = float(js_fov.val);
        }
        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam[max_exposure_ndx].second.as_num();
            max_exposure_ = float(js_max_exposure.val);
        }
    }

    view_origin_ = initial_view_pos_;
    view_dir_ = initial_view_dir_;
}

void PhyTest::Exit() { BaseState::Exit(); }

void PhyTest::Draw() { BaseState::Draw(); }

void PhyTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) { BaseState::DrawUI(r, root); }

void PhyTest::UpdateFixed(const uint64_t dt_us) {
    using namespace PhyTestInternal;

    BaseState::UpdateFixed(dt_us);

    const Ren::Vec3f up = Ren::Vec3f{0, 1, 0}, side = Normalize(Cross(view_dir_, up));

    {
        const float fwd_speed =
                        std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
                    side_speed =
                        std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        view_origin_ += Ren::Vec3d(view_dir_) * fwd_speed;
        view_origin_ += Ren::Vec3d(side) * side_speed;

        if (std::abs(fwd_speed) > 0 || std::abs(side_speed) > 0) {
            invalidate_view_ = true;
        }
    }
}

bool PhyTest::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    using namespace Ren;
    using namespace PhyTestInternal;

    // pt switch for touch controls
    if (evt.type == Eng::eInputEvent::P1Down || evt.type == Eng::eInputEvent::P2Down) {
        if (evt.point[0] > float(ren_ctx_->w()) * 0.9f && evt.point[1] < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    // scene_manager_->InitScene_PT();
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
    case Eng::eInputEvent::P1Down:
        if (evt.point[0] < (float(ren_ctx_->w()) / 3) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case Eng::eInputEvent::P2Down:
        if (evt.point[0] < (float(ren_ctx_->w()) / 3) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case Eng::eInputEvent::P1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case Eng::eInputEvent::P2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case Eng::eInputEvent::P1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, -0.005f * evt.move[0], up);
            rot = Rotate(rot, 0.005f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::eInputEvent::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{0, 1, 0};
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move[0], up);
            rot = Rotate(rot, -0.01f * evt.move[1], side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::eInputEvent::KeyDown: {
        if (evt.key_code == Eng::eKey::Up ||
            (evt.key_code == Eng::eKey::W && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Down ||
                   (evt.key_code == Eng::eKey::S && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Left ||
                   (evt.key_code == Eng::eKey::A && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Right ||
                   (evt.key_code == Eng::eKey::D && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Space) {
        } else {
            input_processed = false;
        }
    } break;
    case Eng::eInputEvent::KeyUp: {
        if (!cmdline_ui_->enabled || view_pointer_) {
            if (evt.key_code == Eng::eKey::Up || evt.key_code == Eng::eKey::W || evt.key_code == Eng::eKey::Down ||
                evt.key_code == Eng::eKey::S) {
                fwd_press_speed_ = 0;
            } else if (evt.key_code == Eng::eKey::Left || evt.key_code == Eng::eKey::A ||
                       evt.key_code == Eng::eKey::Right || evt.key_code == Eng::eKey::D) {
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
        BaseState::HandleInput(evt, keys_state);
    }

    return true;
}

void PhyTest::UpdateAnim(const uint64_t dt_us) {
    const float delta_time_s = dt_us * 0.000001f;

    // Update camera
    scene_manager_->SetupView(view_origin_, view_origin_ + Ren::Vec3d(view_dir_), Ren::Vec3f{0, 1, 0}, view_fov_,
                              Ren::Vec2f{0.0f}, 1, min_exposure_, max_exposure_);

    // log_->Info("%f %f %f | %f %f %f",
    //        view_origin_[0], view_origin_[1], view_origin_[2],
    //        view_dir_[0], view_dir_[1], view_dir_[2]);
}

void PhyTest::SaveScene(Sys::JsObjectP &js_scene) {
    BaseState::SaveScene(js_scene);

    const auto &alloc = js_scene.elements.get_allocator();

    { // write camera
        Sys::JsObjectP js_camera(alloc);

        { // write view origin
            Sys::JsArrayP js_view_origin(alloc);
            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[0]});
            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[1]});
            js_view_origin.Push(Sys::JsNumber{initial_view_pos_[2]});

            js_camera.Insert("view_origin", std::move(js_view_origin));
        }

        { // write view direction
            Sys::JsArrayP js_view_dir(alloc);
            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[0]});
            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[1]});
            js_view_dir.Push(Sys::JsNumber{initial_view_dir_[2]});

            js_camera.Insert("view_dir", std::move(js_view_dir));
        }

        // write forward speed
        js_camera.Insert("fwd_speed", Sys::JsNumber{max_fwd_speed_});

        // write fov
        js_camera.Insert("fov", Sys::JsNumber{view_fov_});

        // write max exposure
        js_camera.Insert("max_exposure", Sys::JsNumber{max_exposure_});

        js_scene.Insert("camera", std::move(js_camera));
    }
}
