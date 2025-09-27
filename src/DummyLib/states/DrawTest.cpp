#include "DrawTest.h"

#include <memory>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/Renderer.h>
#include <Ray/Ray.h>
#include <Ren/Context.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../widgets/FontStorage.h"

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#include <tinyexr/tinyexr.h>
#undef GetObject

#include <optick/optick.h>
#include <stb/stb_image.h>

namespace Ray {
extern const int LUT_DIMS;
extern const uint32_t *transform_luts[];
} // namespace Ray

namespace DrawTestInternal {
Ren::Span<const uint8_t> RayLUTByName(const std::string_view name) {
    const uint32_t *lut_data = nullptr;
    if (name == "agx") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::AgX)];
    } else if (name == "agx_punchy") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::AgX_Punchy)];
    } else if (name == "filmic_very_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_VeryLowContrast)];
    } else if (name == "filmic_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_LowContrast)];
    } else if (name == "filmic_med_low_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumLowContrast)];
    } else if (name == "filmic_med_contrast" || name == "filmic") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumContrast)];
    } else if (name == "filmic_med_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_MediumHighContrast)];
    } else if (name == "filmic_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_HighContrast)];
    } else if (name == "filmic_very_high_contrast") {
        lut_data = Ray::transform_luts[int(Ray::eViewTransform::Filmic_VeryHighContrast)];
    }
    return Ren::Span<const uint8_t>(reinterpret_cast<const uint8_t *>(lut_data),
                                    4 * Ray::LUT_DIMS * Ray::LUT_DIMS * Ray::LUT_DIMS);
}
} // namespace DrawTestInternal

#include <Ren/Utils.h>

namespace SceneManagerInternal {
int WriteImage(const uint8_t *out_data, int w, int h, int channels, bool flip_y, bool is_rgbm, const char *name);
}

DrawTest::DrawTest(Viewer *viewer) : BaseState(viewer) {}

void DrawTest::Enter() {
    using namespace DrawTestInternal;

    BaseState::Enter();

    cmdline_ui_->RegisterCommand("r_printCam", [this](Ren::Span<const Eng::CmdlineUI::ArgData> args) -> bool {
        log_->Info("View Pos: { %f, %f, %f }", view_origin_[0], view_origin_[1], view_origin_[2]);
        log_->Info("View Dir: { %f, %f, %f }", view_dir_[0], view_dir_[1], view_dir_[2]);
        return true;
    });

    log_->Info("DrawTest: Loading scene!");
    BaseState::LoadScene(viewer_->app_params.scene_name);
}

void DrawTest::OnPreloadScene(Sys::JsObjectP &js_scene) {
    BaseState::OnPreloadScene(js_scene);

    std::fill_n(wolf_indices_, 32, 0xffffffff);
    std::fill_n(scooter_indices_, 16, 0xffffffff);
    std::fill_n(sophia_indices_, 2, 0xffffffff);
    std::fill_n(eric_indices_, 2, 0xffffffff);
    zenith_index_ = 0xffffffff;
    palm_index_ = 0xffffffff;
    leaf_tree_index_ = 0xffffffff;
}

void DrawTest::OnPostloadScene(Sys::JsObjectP &js_scene) {
    using namespace DrawTestInternal;

    BaseState::OnPostloadScene(js_scene);

    cam_frames_.clear();
    cam_frame_ = -1;

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
        } else if (const size_t view_rot_ndx = js_cam.IndexOf("view_rot"); view_rot_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_view_rot = js_cam[view_rot_ndx].second.as_arr();

            auto rx = float(js_view_rot.at(0).as_num().val);
            auto ry = float(js_view_rot.at(1).as_num().val);
            auto rz = float(js_view_rot.at(2).as_num().val);

            rx *= Ren::Pi<float>() / 180;
            ry *= Ren::Pi<float>() / 180;
            rz *= Ren::Pi<float>() / 180;

            Ren::Mat4f transform;
            transform = Rotate(transform, float(rz), Ren::Vec3f{0, 0, 1});
            transform = Rotate(transform, float(rx), Ren::Vec3f{1, 0, 0});
            transform = Rotate(transform, float(ry), Ren::Vec3f{0, 1, 0});

            auto view_vec = Ren::Vec4f{0, -1, 0, 0};
            view_vec = transform * view_vec;

            initial_view_dir_ = Ren::Vec3d{view_vec};
        }

        if (const size_t fwd_speed_ndx = js_cam.IndexOf("fwd_speed"); fwd_speed_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fwd_speed = js_cam[fwd_speed_ndx].second.as_num();
            max_fwd_speed_ = float(js_fwd_speed.val);
        }

        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            view_fov_ = float(js_fov.val);
        }

        if (const size_t gamma_ndx = js_cam.IndexOf("gamma"); gamma_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_gamma = js_cam[gamma_ndx].second.as_num();
            gamma_ = float(js_gamma.val);
        }

        if (const size_t min_exposure_ndx = js_cam.IndexOf("min_exposure"); min_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_min_exposure = js_cam[min_exposure_ndx].second.as_num();
            min_exposure_ = float(js_min_exposure.val);
        }

        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam[max_exposure_ndx].second.as_num();
            max_exposure_ = float(js_max_exposure.val);
        }

        if (const size_t shift_ndx = js_cam.IndexOf("shift"); shift_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_shift = js_cam[shift_ndx].second.as_arr();
            view_sensor_shift_[0] = float(js_shift[0].as_num().val);
            view_sensor_shift_[1] = float(js_shift[1].as_num().val);
        }

        if (const size_t filter_ndx = js_cam.IndexOf("filter"); filter_ndx < js_cam.Size()) {
            const Sys::JsStringP &js_filter = js_cam[filter_ndx].second.as_str();
            if (js_filter.val == "box") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::Box;
            } else if (js_filter.val == "gaussian") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::Gaussian;
            } else if (js_filter.val == "blackman-harris") {
                renderer_->settings.pixel_filter = Eng::ePixelFilter::BlackmanHarris;
                renderer_->settings.pixel_filter_width = 1.5f;
                if (const size_t filter_width_ndx = js_cam.IndexOf("filter_width"); filter_width_ndx < js_cam.Size()) {
                    const Sys::JsNumber &js_filter_width = js_cam[filter_width_ndx].second.as_num();
                    renderer_->settings.pixel_filter_width = float(js_filter_width.val);
                }
            }
        }

        if (const size_t view_transform_ndx = js_cam.IndexOf("view_transform"); view_transform_ndx < js_cam.Size()) {
            const Sys::JsStringP &js_view_transform = js_cam[view_transform_ndx].second.as_str();
            if (js_view_transform.val == "standard") {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Standard;
            } else if (js_view_transform.val == "off") {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::Off;
            } else {
                renderer_->settings.tonemap_mode = Eng::eTonemapMode::LUT;
                renderer_->SetTonemapLUT(Ray::LUT_DIMS, Ren::eTexFormat::RGB10_A2, RayLUTByName(js_view_transform.val));
            }
        }

        if (const size_t paths_ndx = js_cam.IndexOf("paths");
            paths_ndx < js_cam.Size() && viewer_->app_params.cam_path.has_value()) {
            const Sys::JsArrayP &js_frames =
                js_cam[paths_ndx].second.as_arr().at(*viewer_->app_params.cam_path).as_arr();
            for (const Sys::JsElementP &el : js_frames.elements) {
                const Sys::JsObjectP &js_frame = el.as_obj();

                const Sys::JsArrayP &js_frame_pos = js_frame.at("pos").as_arr();
                const Sys::JsArrayP &js_frame_rot = js_frame.at("rot").as_arr();

                auto rx = float(js_frame_rot.at(0).as_num().val);
                auto ry = float(js_frame_rot.at(1).as_num().val);
                auto rz = float(js_frame_rot.at(2).as_num().val);

                rx *= Ren::Pi<float>() / 180;
                ry *= Ren::Pi<float>() / 180;
                rz *= Ren::Pi<float>() / 180;

                Ren::Mat4f transform;
                transform = Rotate(transform, float(rz), Ren::Vec3f{0, 0, 1});
                transform = Rotate(transform, float(rx), Ren::Vec3f{1, 0, 0});
                transform = Rotate(transform, float(ry), Ren::Vec3f{0, 1, 0});

                auto view_vec = Ren::Vec4f{0, -1, 0, 0};
                view_vec = transform * view_vec;

                cam_frame_t &frame = cam_frames_.emplace_back();
                frame.pos[0] = js_frame_pos.at(0).as_num().val;
                frame.pos[1] = js_frame_pos.at(1).as_num().val;
                frame.pos[2] = js_frame_pos.at(2).as_num().val;
                frame.dir = Ren::Vec3d(view_vec);
            }
        }
    }

    if (!cam_frames_.empty()) {
        // Use the first frame as initial state
        initial_view_pos_ = cam_frames_[0].pos;
        initial_view_dir_ = cam_frames_[0].dir;
    }

    if (viewer_->app_params.exposure.has_value()) {
        min_exposure_ = max_exposure_ = viewer_->app_params.exposure.value();
    }

    next_view_origin_ = prev_view_origin_ = initial_view_pos_;
    view_dir_ = initial_view_dir_;

    Eng::SceneData &scene = scene_manager_->scene_data();

    {
        char wolf_name[] = "wolf00";

        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 8; i++) {
                const int index = j * 8 + i;

                wolf_name[4] = char('0' + j);
                wolf_name[5] = char('0' + i);

                const uint32_t wolf_index = scene_manager_->FindObject(wolf_name);
                wolf_indices_[index] = wolf_index;

                if (wolf_index != 0xffffffff) {
                    Eng::SceneObject *wolf = scene_manager_->GetObject(wolf_index);

                    const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
                    if ((wolf->comp_mask & mask) == mask) {
                        auto *as = (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(
                            wolf->components[Eng::CompAnimState]);
                        as->anim_time_s = 4 * (float(rand()) / float(RAND_MAX)); // NOLINT
                    }
                }
            }
        }
    }

    {
        char scooter_name[] = "scooter_00";

        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 8; i++) {
                scooter_name[8] = char('0' + j);
                scooter_name[9] = char('0' + i);

                const uint32_t scooter_index = scene_manager_->FindObject(scooter_name);
                scooter_indices_[j * 8 + i] = scooter_index;
            }
        }
    }

    {
        char sophia_name[] = "sophia_00";

        for (int i = 0; i < 2; i++) {
            sophia_name[8] = char('0' + i);

            const uint32_t sophia_index = scene_manager_->FindObject(sophia_name);
            sophia_indices_[i] = sophia_index;
        }
    }

    {
        char eric_name[] = "eric_00";

        for (int i = 0; i < 2; i++) {
            eric_name[6] = char('0' + i);

            const uint32_t eric_index = scene_manager_->FindObject(eric_name);
            eric_indices_[i] = eric_index;
        }
    }

    zenith_index_ = scene_manager_->FindObject("zenith");
    palm_index_ = scene_manager_->FindObject("palm");
    leaf_tree_index_ = scene_manager_->FindObject("leaf_tree");

    dynamic_light_index_ = scene_manager_->FindObject("__dynamic_test_light");
}

void DrawTest::Exit() { BaseState::Exit(); }

void DrawTest::Draw() { BaseState::Draw(); }

void DrawTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) { BaseState::DrawUI(r, root); }

void DrawTest::UpdateFixed(const uint64_t dt_us) {
    using namespace DrawTestInternal;

    OPTICK_EVENT("DrawTest::UpdateFixed");
    BaseState::UpdateFixed(dt_us);

    const Ren::Vec3d up = Ren::Vec3d{0, 1, 0}, side = Normalize(Cross(view_dir_, up));

    prev_view_origin_ = next_view_origin_;

    if (cam_frames_.empty()) {
        const float fwd_speed =
                        std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
                    side_speed =
                        std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        next_view_origin_ += view_dir_ * fwd_speed;
        next_view_origin_ += side * side_speed;
    }

    Eng::SceneData &scene = scene_manager_->scene_data();

    if (scooter_indices_[0] != 0xffffffff) {
        const auto rot_center = Ren::Vec3f{44.5799f, 0.15f, 24.7763f};

        scooters_angle_ += 0.0000005f * dt_us;

        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) {
                break;
            }

            Eng::SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            const uint32_t mask = Eng::CompTransform;
            if ((scooter->comp_mask & mask) == mask) {
                auto *tr = (Eng::Transform *)scene.comp_store[Eng::CompTransform]->Get(
                    scooter->components[Eng::CompTransform]);

                tr->world_from_object_prev = tr->world_from_object;
                tr->world_from_object = Ren::Mat4f{1};
                tr->world_from_object = Translate(tr->world_from_object, rot_center);

                if (i < 8) {
                    // inner circle
                    tr->world_from_object =
                        Rotate(tr->world_from_object, scooters_angle_ + float(i) * 0.25f * Ren::Pi<float>(),
                               Ren::Vec3f{0, 1, 0});
                    tr->world_from_object = Translate(tr->world_from_object, Ren::Vec3f{6.5f, 0.0f, 0.0f});
                } else {
                    // outer circle
                    tr->world_from_object =
                        Rotate(tr->world_from_object, -scooters_angle_ + float(i - 8) * 0.25f * Ren::Pi<float>(),
                               Ren::Vec3f{0, 1, 0});
                    tr->world_from_object = Translate(tr->world_from_object, Ren::Vec3f{-8.5f, 0.0f, 0.0f});
                }
            }
        }

        scene_manager_->InvalidateObjects(scooter_indices_, Eng::CompTransformBit);
    }

    if (dynamic_light_index_ != 0xffffffff) {
        Eng::SceneObject *light = scene_manager_->GetObject(dynamic_light_index_);

        light_angle_ += 0.000001f * dt_us;

        const uint32_t mask = Eng::CompTransform;
        if ((light->comp_mask & mask) == mask) {
            auto *tr =
                (Eng::Transform *)scene.comp_store[Eng::CompTransform]->Get(light->components[Eng::CompTransform]);

            tr->world_from_object_prev = tr->world_from_object;
            tr->world_from_object = Ren::Mat4f{1};

            tr->world_from_object = Rotate(tr->world_from_object, light_angle_, Ren::Vec3f{0, 1, 0});
            tr->world_from_object = Translate(tr->world_from_object, Ren::Vec3f{0.4f, 0.5f, 0.0f});
        }

        scene_manager_->InvalidateObjects({&dynamic_light_index_, 1}, Eng::CompTransformBit);
    }

    wind_update_time_ += dt_us;

    if (wind_update_time_ > 400000) {
        wind_update_time_ = 0;
        // update wind vector
        // const float next_wind_strength = 0.15f * random_->GetNormalizedFloat();
        // wind_vector_goal_ = next_wind_strength * random_->GetUnitVec3();
    }

    scene.env.wind_vec = 0.99f * scene.env.wind_vec + 0.01f * wind_vector_goal_;
    scene.env.wind_turbulence = 2 * Length(scene.env.wind_vec);
}

void DrawTest::UpdateAnim(const uint64_t dt_us) {
    using namespace Ren;

    OPTICK_EVENT();
    const float delta_time_s = dt_us * 0.000001f;

    // test test test
    TestUpdateAnims(delta_time_s);

    const Eng::FrameInfo &fr = fr_info_;
    Vec3d smooth_view_origin = Mix(prev_view_origin_, next_view_origin_, fr.time_fract);

    if (cam_frame_ != -1 && cam_frame_ < cam_frames_.size()) {
        smooth_view_origin = cam_frames_[cam_frame_].pos;
        view_dir_ = cam_frames_[cam_frame_].dir;
    }

    // Invalidate view if camera has moved
    invalidate_view_ |= Distance(view_origin_, smooth_view_origin) > 0.00001;
    view_origin_ = smooth_view_origin;

    // Make sure we use latest camera rotation (reduce lag)
    Vec3d view_dir = view_dir_;
    int view_pointer = view_pointer_;
    for (const Eng::input_event_t &evt : viewer_->input_manager()->peek_events()) {
        switch (evt.type) {
        case Eng::eInputEvent::P1Down:
            if (view_pointer == 0) {
                view_pointer = 1;
            }
            break;
        case Eng::eInputEvent::P2Down:
            if (view_pointer == 0) {
                view_pointer = 2;
            }
            break;
        case Eng::eInputEvent::P1Up:
            if (view_pointer == 1) {
                view_pointer = 0;
            }
            break;
        case Eng::eInputEvent::P2Up:
            if (view_pointer == 2) {
                view_pointer = 0;
            }
            break;
        case Eng::eInputEvent::P1Move:
            if (view_pointer == 1) {
                auto up = Vec3d{0, 1, 0};
                Vec3d side = Normalize(Cross(view_dir, up));
                up = Cross(side, view_dir);

                Mat4d rot;
                rot = Rotate(rot, -0.005 * evt.move[0], up);
                rot = Rotate(rot, 0.005 * evt.move[1], side);

                auto rot_m3 = Mat3d(rot);
                view_dir = rot_m3 * view_dir;

                invalidate_view_ = true;
            }
            break;
        case Eng::eInputEvent::P2Move:
            if (view_pointer == 2) {
                auto up = Vec3d{0, 1, 0};
                Vec3d side = Normalize(Cross(view_dir, up));
                up = Cross(side, view_dir);

                Mat4d rot;
                rot = Rotate(rot, 0.01 * evt.move[0], up);
                rot = Rotate(rot, -0.01 * evt.move[1], side);

                auto rot_m3 = Mat3d(rot);
                view_dir = rot_m3 * view_dir;

                invalidate_view_ = true;
            }
            break;
        default:
            break;
        }
    }

    const bool view_locked = !viewer_->app_params.ref_name.empty();
    if (view_locked) {
        view_dir = view_dir_;
    }

    // Update camera
    scene_manager_->SetupView(view_origin_, view_origin_ + view_dir, Vec3f{0, 1, 0}, view_fov_, view_sensor_shift_,
                              gamma_, min_exposure_, max_exposure_);

    BaseState::UpdateAnim(dt_us);
}

bool DrawTest::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    using namespace Ren;
    using namespace DrawTestInternal;

    const bool handled = BaseState::HandleInput(evt, keys_state);
    if (handled) {
        return true;
    }

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

    const Ren::Vec3d view_dir_before = view_dir_;

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
        } else if ((keys_state[Eng::eKey::LeftShift] || keys_state[Eng::eKey::RightShift]) && sun_pointer_ == 0) {
            sun_pointer_ = 2;
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
        } else if (sun_pointer_ == 2) {
            sun_pointer_ = 0;
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
            auto up = Vec3d{0, 1, 0};
            Vec3d side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4d rot;
            rot = Rotate(rot, -0.005 * evt.move[0], up);
            rot = Rotate(rot, 0.005 * evt.move[1], side);

            auto rot_m3 = Mat3d(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        } else if (sun_pointer_ == 2) {
            auto up = Vec3d{0, 1, 0};
            Vec3d side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4d rot;
            rot = Rotate(rot, -0.005 * evt.move[0], up);
            rot = Rotate(rot, 0.005 * evt.move[1], side);

            auto rot_m3 = Mat3d(rot);
            sun_dir_ = Vec3f(Normalize(rot_m3 * Vec3d(sun_dir_)));
        }
        break;
    case Eng::eInputEvent::P2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move[0] * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ += evt.move[1] * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3d{0, 1, 0};
            Vec3d side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4d rot;
            rot = Rotate(rot, 0.01 * evt.move[0], up);
            rot = Rotate(rot, -0.01 * evt.move[1], side);

            auto rot_m3 = Mat3d(rot);
            view_dir_ = rot_m3 * view_dir_;

            invalidate_view_ = true;
        }
        break;
    case Eng::eInputEvent::KeyDown: {
        if ((evt.key_code == Eng::eKey::Up && !cmdline_ui_->enabled) ||
            (evt.key_code == Eng::eKey::W && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if ((evt.key_code == Eng::eKey::Down && !cmdline_ui_->enabled) ||
                   (evt.key_code == Eng::eKey::S && (!cmdline_ui_->enabled || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Left ||
                   (evt.key_code == Eng::eKey::A && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == Eng::eKey::Right ||
                   (evt.key_code == Eng::eKey::D && (!cmdline_ui_->enabled || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
            //} else if (evt.key_code == KeySpace) {
            //    wind_vector_goal_ = Ren::Vec3f{1, 0, 0};
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
            }
        }
    } break;
    default:
        break;
    }

    const bool view_locked = !viewer_->app_params.ref_name.empty();
    if (view_locked) {
        view_dir_ = view_dir_before;
        fwd_touch_speed_ = side_touch_speed_ = 0;
        fwd_press_speed_ = side_press_speed_ = 0;
    }

    return true;
}

void DrawTest::SaveScene(Sys::JsObjectP &js_scene) {
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

        { // write forward speed
            js_camera.Insert("fwd_speed", Sys::JsNumber{max_fwd_speed_});
        }

        { // write fov
            js_camera.Insert("fov", Sys::JsNumber{view_fov_});
        }

        { // write max exposure
            js_camera.Insert("max_exposure", Sys::JsNumber{max_exposure_});
        }

        js_scene.Insert("camera", std::move(js_camera));
    }
}

void DrawTest::TestUpdateAnims(const float delta_time_s) {
    Eng::SceneData &scene = scene_manager_->scene_data();

    if (wolf_indices_[0] != 0xffffffff) {
        for (uint32_t wolf_index : wolf_indices_) {
            if (wolf_index == 0xffffffff) {
                break;
            }

            Eng::SceneObject *wolf = scene_manager_->GetObject(wolf_index);

            const uint32_t mask = Eng::CompTransformBit | Eng::CompDrawableBit | Eng::CompAnimStateBit;
            if ((wolf->comp_mask & mask) == mask) {
                auto *tr =
                    (Eng::Transform *)scene.comp_store[Eng::CompTransform]->Get(wolf->components[Eng::CompTransform]);
                auto *dr =
                    (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(wolf->components[Eng::CompDrawable]);
                auto *as =
                    (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(wolf->components[Eng::CompAnimState]);

                // keep previous palette for velocity calculation
                std::swap(as->matr_palette_curr, as->matr_palette_prev);
                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(&as->matr_palette_curr[0]);

                {
                    Ren::Mat4f xform;
                    // xform = Rotate(xform, 0.5f * delta_time_s, Ren::Vec3f{0, 1, 0});
                    xform = Translate(xform, delta_time_s * Ren::Vec3f{0.1f, 0.0f, 0.0f});

                    tr->world_from_object_prev = tr->world_from_object;
                    tr->world_from_object = xform * tr->world_from_object;
                    scene_manager_->InvalidateObjects({&wolf_index, 1}, Eng::CompTransformBit);
                }
            }
        }
    }

    if (scooter_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) {
                break;
            }

            Eng::SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
            if ((scooter->comp_mask & mask) == mask) {
                auto *dr =
                    (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(scooter->components[Eng::CompDrawable]);
                auto *as = (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(
                    scooter->components[Eng::CompAnimState]);

                // keep previous palette for velocity calculation
                std::swap(as->matr_palette_curr, as->matr_palette_prev);
                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = (i < 8) ? 2 : 3;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(&as->matr_palette_curr[0]);
            }
        }
    }

    for (const uint32_t ndx : sophia_indices_) {
        if (ndx == 0xffffffff) {
            break;
        }

        Eng::SceneObject *sophia = scene_manager_->GetObject(ndx);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((sophia->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(sophia->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(sophia->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);

            sophia->change_mask |= Eng::CompDrawableBit;
        }
    }

    for (const uint32_t ndx : eric_indices_) {
        if (ndx == 0xffffffff) {
            break;
        }

        Eng::SceneObject *eric = scene_manager_->GetObject(ndx);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((eric->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(eric->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(eric->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    if (zenith_index_ != 0xffffffff) {
        Eng::SceneObject *zenith = scene_manager_->GetObject(zenith_index_);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((zenith->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(zenith->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(zenith->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    if (palm_index_ != 0xffffffff) {
        Eng::SceneObject *palm = scene_manager_->GetObject(palm_index_);

        const uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((palm->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(palm->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(palm->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }

    /*if (leaf_tree_index_ != 0xffffffff) {
        SceneObject *leaf_tree = scene_manager_->GetObject(leaf_tree_index_);

        uint32_t mask = CompDrawableBit | CompTransformBit;
        if ((leaf_tree->comp_mask & mask) == mask) {
            auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(leaf_tree->components[CompTransform]);

            Ren::Mat4f rot_mat;
            rot_mat = Rotate(rot_mat, 0.5f * delta_time_s, Ren::Vec3f{0, 1, 0});
            //rot_mat = Translate(rot_mat, Ren::Vec3f{0.001f, 0.0f, 0.0f});

            tr->world_from_object_prev = tr->world_from_object;
            tr->world_from_object = rot_mat * tr->world_from_object;
            scene_manager_->InvalidateObjects(&leaf_tree_index_, 1, CompTransformBit);
        }
    }*/

    const auto wind_scroll_dir = 128 * Normalize(Ren::Vec2f{scene.env.wind_vec[0], scene.env.wind_vec[2]});
    scene.env.prev_wind_scroll_lf = scene.env.curr_wind_scroll_lf;
    scene.env.prev_wind_scroll_hf = scene.env.curr_wind_scroll_hf;

    scene.env.curr_wind_scroll_lf =
        Fract(scene.env.curr_wind_scroll_lf - (1.0f / 1536.0f) * delta_time_s * wind_scroll_dir);
    scene.env.curr_wind_scroll_hf =
        Fract(scene.env.curr_wind_scroll_hf - (1.0f / 32.0f) * delta_time_s * wind_scroll_dir);
}

#undef TINYEXR_IMPLEMENTATION
#undef TINYEXR_USE_MINIZ
#undef TINYEXR_USE_STB_ZLIB
