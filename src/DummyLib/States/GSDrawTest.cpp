#include "GSDrawTest.h"

#include <fstream>
#include <memory>

#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Eng/Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Viewer.h"

namespace GSDrawTestInternal {
#if defined(__ANDROID__)
    const char SCENE_NAME[] = "assets/scenes/"
#else
    const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
        //"font_test.json";
        //"skin_test.json";
        //"living_room_gumroad.json";
        //"bistro.json";
        //"pbr_test.json";
        //"zenith.json";
        //"corridor.json";
        "vegetation_test.json";
}

GSDrawTest::GSDrawTest(GameBase *game) : GSBaseState(game) {
}

void GSDrawTest::Enter() {
    using namespace GSDrawTestInternal;

    GSBaseState::Enter();

    log_->Info("GSDrawTest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);
}

void GSDrawTest::OnPreloadScene(JsObject &js_scene) {
    GSBaseState::OnPreloadScene(js_scene);

#if 0
    JsArray &js_objects = js_scene.at("objects").as_arr();
    if (js_objects.elements.size() < 2) return;

    JsObject 
        js_leaf_tree = js_objects.elements.begin()->as_obj(),
        js_palm_tree = (++js_objects.elements.begin())->as_obj();
    if (!js_leaf_tree.Has("name") || !js_palm_tree.Has("name")) return;

    if (js_leaf_tree.at("name").as_str().val != "leaf_tree" ||
        js_palm_tree.at("name").as_str().val != "palm_tree") return;
    
    for (int j = -9; j < 10; j++) {
        for (int i = -9; i < 10; i++) {
            if (j == 0 && i == 0) continue;

            js_leaf_tree.at("name").as_str().val = "leaf " + std::to_string(j) + ":" + std::to_string(i);
            js_palm_tree.at("name").as_str().val = "palm " + std::to_string(j) + ":" + std::to_string(i);

            {   // set leaf tree position
                JsObject &js_leaf_tree_tr = js_leaf_tree.at("transform").as_obj();
                JsArray &js_leaf_tree_pos = js_leaf_tree_tr.at("pos").as_arr();
                JsArray &js_leaf_tree_rot = js_leaf_tree_tr.at("rot").as_arr();

                JsNumber &js_leaf_posx = js_leaf_tree_pos.elements.begin()->as_num();
                JsNumber &js_leaf_posz = (++(++js_leaf_tree_pos.elements.begin()))->as_num();

                js_leaf_posx.val = double(i * 10);
                js_leaf_posz.val = double(j * 10);

                JsNumber &js_leaf_roty = (++js_leaf_tree_rot.elements.begin())->as_num();
                js_leaf_roty.val = double(i) * 43758.5453;
            }

            {   // set palm tree position
                JsObject &js_palm_tree_tr = js_palm_tree.at("transform").as_obj();
                JsArray &js_palm_tree_pos = js_palm_tree_tr.at("pos").as_arr();
                JsArray &js_palm_tree_rot = js_palm_tree_tr.at("rot").as_arr();

                JsNumber &js_palm_posx = js_palm_tree_pos.elements.begin()->as_num();
                JsNumber &js_palm_posz = (++(++js_palm_tree_pos.elements.begin()))->as_num();

                js_palm_posx.val = double(i * 10) + 5.0;
                js_palm_posz.val = double(j * 10);

                JsNumber &js_palm_roty = (++js_palm_tree_rot.elements.begin())->as_num();
                js_palm_roty.val = double(i) * 12.9898;
            }

            js_objects.elements.push_back(js_leaf_tree);
            js_objects.elements.push_back(js_palm_tree);
        }
    }

    //std::ofstream test_out_file("C:\\repos\\DummyEngine\\assets\\scenes\\veg_test.json", std::ios::binary);
    //js_scene.Write(test_out_file);
#endif
}

void GSDrawTest::OnPostloadScene(JsObject &js_scene) {
    GSBaseState::OnPostloadScene(js_scene);

    cam_follow_path_.clear();
    cam_follow_point_ = 0;
    cam_follow_param_ = 0.0f;

    if (js_scene.Has("camera")) {
        const JsObject &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArray &js_orig = js_cam.at("view_origin").as_arr();
            initial_view_origin_[0] = (float)js_orig.at(0).as_num().val;
            initial_view_origin_[1] = (float)js_orig.at(1).as_num().val;
            initial_view_origin_[2] = (float)js_orig.at(2).as_num().val;
        }

        if (js_cam.Has("view_dir")) {
            const JsArray &js_dir = js_cam.at("view_dir").as_arr();
            initial_view_dir_[0] = (float)js_dir.at(0).as_num().val;
            initial_view_dir_[1] = (float)js_dir.at(1).as_num().val;
            initial_view_dir_[2] = (float)js_dir.at(2).as_num().val;
        }

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            max_fwd_speed_ = (float)js_fwd_speed.val;
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov_ = (float)js_fov.val;
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure_ = (float)js_max_exposure.val;
        }

        if (js_cam.Has("follow_path")) {
            const JsArray &js_points = js_cam.at("follow_path").as_arr();
            for (const JsElement &el : js_points.elements) {
                const JsArray &js_point = el.as_arr();

                const JsNumber
                    &x = js_point.at(0).as_num(),
                    &y = js_point.at(1).as_num(),
                    &z = js_point.at(2).as_num();

                cam_follow_path_.emplace_back((float)x.val, (float)y.val, (float)z.val);
            }
        }
    }

    view_origin_ = initial_view_origin_;
    view_dir_ = initial_view_dir_;

    SceneData &scene = scene_manager_->scene_data();

    {
        char wolf_name[] = "wolf00";

        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 8; i++) {
                int index = j * 8 + i;

                wolf_name[4] = char('0' + j);
                wolf_name[5] = char('0' + i);

                uint32_t wolf_index = scene_manager_->FindObject(wolf_name);
                wolf_indices_[index] = wolf_index;

                if (wolf_index != 0xffffffff) {
                    SceneObject *wolf = scene_manager_->GetObject(wolf_index);

                    uint32_t mask = CompDrawableBit | CompAnimStateBit;
                    if ((wolf->comp_mask & mask) == mask) {
                        auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(wolf->components[CompAnimState]);
                        as->anim_time_s = 4.0f * (float(rand()) / RAND_MAX);
                    }
                }
            }
        }
    }

    {
        font_meshes_[0] = scene_manager_->FindObject("font_mesh0");
        font_meshes_[1] = scene_manager_->FindObject("font_mesh1");
        font_meshes_[2] = scene_manager_->FindObject("font_mesh2");
        font_meshes_[3] = scene_manager_->FindObject("font_mesh3");
    }

    {
        char scooter_name[] = "scooter_00";

        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 8; i++) {
                scooter_name[8] = char('0' + j);
                scooter_name[9] = char('0' + i);

                uint32_t scooter_index = scene_manager_->FindObject(scooter_name);
                scooter_indices_[j * 8 + i] = scooter_index;
            }
        }
    }

    {
        char sophia_name[] = "sophia_00";

        for (int i = 0; i < 2; i++) {
            sophia_name[8] = char('0' + i);

            uint32_t sophia_index = scene_manager_->FindObject(sophia_name);
            sophia_indices_[i] = sophia_index;
        }
    }

    {
        char eric_name[] = "eric_00";

        for (int i = 0; i < 2; i++) {
            eric_name[6] = char('0' + i);

            uint32_t eric_index = scene_manager_->FindObject(eric_name);
            eric_indices_[i] = eric_index;
        }
    }

    zenith_index_ = scene_manager_->FindObject("zenith");
    palm_index_ = scene_manager_->FindObject("palm");
    leaf_tree_index_ = scene_manager_->FindObject("leaf_tree");
}

void GSDrawTest::Exit() {
    GSBaseState::Exit();
}

void GSDrawTest::Draw(uint64_t dt_us) {
    GSBaseState::Draw(dt_us);
}

void GSDrawTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    GSBaseState::DrawUI(r, root);
}

void GSDrawTest::Update(uint64_t dt_us) {
    using namespace GSDrawTestInternal;

    const Ren::Vec3f
        up = Ren::Vec3f{ 0, 1, 0 },
        side = Normalize(Cross(view_dir_, up));

    if (cam_follow_path_.size() < 3) {
        const float
            fwd_speed = std::max(std::min(fwd_press_speed_ + fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_),
            side_speed = std::max(std::min(side_press_speed_ + side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

        view_origin_ += view_dir_ * fwd_speed;
        view_origin_ += side * side_speed;

        if (std::abs(fwd_speed) > 0.0f || std::abs(side_speed) > 0.0f) {
            invalidate_view_ = true;
        }
    } else {
        int next_point = (cam_follow_point_ + 1) % (int)cam_follow_path_.size();

        {   // update param
            const Ren::Vec3f
                &p1 = cam_follow_path_[cam_follow_point_],
                &p2 = cam_follow_path_[next_point];

            cam_follow_param_ += 0.000005f * dt_us / Ren::Distance(p1, p2);
            while (cam_follow_param_ > 1.0f) {
                cam_follow_point_ = (cam_follow_point_ + 1) % (int)cam_follow_path_.size();
                cam_follow_param_ -= 1.0f;
            }
        }

        next_point = (cam_follow_point_ + 1) % (int)cam_follow_path_.size();

        const Ren::Vec3f &p1 = cam_follow_path_[cam_follow_point_],
                         &p2 = cam_follow_path_[next_point];

        view_origin_ = 0.95f * view_origin_ + 0.05f * Mix(p1, p2, cam_follow_param_);
        view_dir_ = 0.9f * view_dir_ + 0.1f * Normalize(p2 - view_origin_);
        view_dir_ = Normalize(view_dir_);

        invalidate_view_ = true;
    }

    uint32_t mask = CompTransformBit | CompDrawableBit;
#if 0
    static float t = 0.0f;
    t += 0.04f;

    //const uint32_t monkey_ids[] = { 12, 13, 14, 15, 16 };
    const uint32_t monkey_ids[] = { 28, 29, 30, 31, 32 };

    SceneObject *monkey1 = scene_manager_->GetObject(monkey_ids[0]);
    if ((monkey1->comp_mask & mask) == mask) {
        auto *tr = monkey1->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey2 = scene_manager_->GetObject(monkey_ids[1]);
    if ((monkey2->comp_mask & mask) == mask) {
        auto *tr = monkey2->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(1.5f + t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey3 = scene_manager_->GetObject(monkey_ids[2]);
    if ((monkey3->comp_mask & mask) == mask) {
        auto *tr = monkey3->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(1.5f + t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey4 = scene_manager_->GetObject(monkey_ids[3]);
    if ((monkey4->comp_mask & mask) == mask) {
        auto *tr = monkey4->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    SceneObject *monkey5 = scene_manager_->GetObject(monkey_ids[4]);
    if ((monkey5->comp_mask & mask) == mask) {
        auto *tr = monkey5->tr.get();
        tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 0.0f, 0.0f + 0.02f * std::cos(t), 0.05f + 0.04f * std::cos(t) });
    }

    scene_manager_->InvalidateObjects(monkey_ids, 5, CompTransformBit);
#endif
    SceneData &scene = scene_manager_->scene_data();

    if (scooter_indices_[0] != 0xffffffff) {
        const auto rot_center = Ren::Vec3f{ 44.5799f, 0.15f, 24.7763f };

        scooters_angle_ += 0.0000005f * dt_us;

        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) break;

            SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            uint32_t mask = CompTransform;
            if ((scooter->comp_mask & mask) == mask) {
                auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(scooter->components[CompTransform]);

                tr->mat = Ren::Mat4f{ 1.0f };
                tr->mat = Ren::Translate(tr->mat, rot_center);
                
                if (i < 8) {
                    // inner circle
                    tr->mat = Ren::Rotate(tr->mat, scooters_angle_ + i * 0.25f * Ren::Pi<float>(), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ 6.5f, 0.0f, 0.0f });
                } else {
                    // outer circle
                    tr->mat = Ren::Rotate(tr->mat, -scooters_angle_ + (i - 8) * 0.25f * Ren::Pi<float>(), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, Ren::Vec3f{ -8.5f, 0.0f, 0.0f });
                }
            }
        }

        scene_manager_->InvalidateObjects(scooter_indices_, 16, CompTransformBit);
    }

    wind_update_time_ += dt_us;

    if (wind_update_time_ > 400000) {
        wind_update_time_ = 0;
        // update wind vector
        const float next_wind_strength = 0.15f * random_->GetNormalizedFloat();
        //wind_vector_goal_ = next_wind_strength * random_->GetUnitVec3();
    }

    scene.env.wind_vec = 0.99f * scene.env.wind_vec + 0.01f * wind_vector_goal_;
    scene.env.wind_turbulence = (1.0f / 32.0f) * Ren::Length(scene.env.wind_vec);
}

bool GSDrawTest::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSDrawTestInternal;

    // pt switch for touch controls
    if (evt.type == RawInputEvent::EvP1Down || evt.type == RawInputEvent::EvP2Down) {
        if (evt.point.x > (float)ctx_->w() * 0.9f && evt.point.y < (float)ctx_->h() * 0.1f) {
            uint32_t new_time = Sys::GetTimeMs();
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
    case RawInputEvent::EvP1Down:
        if (evt.point.x < ((float)ctx_->w() / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case RawInputEvent::EvP2Down:
        if (evt.point.x < ((float)ctx_->w() / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case RawInputEvent::EvP1Up:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case RawInputEvent::EvP2Up:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case RawInputEvent::EvP1Move:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            auto up = Vec3f{ 0, 1, 0 };
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
    case RawInputEvent::EvP2Move:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            auto up = Vec3f{ 0, 1, 0 };
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
    case RawInputEvent::EvKeyDown: {
        if (evt.key_code == KeyUp || (evt.key_code == KeyW && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == KeyDown || (evt.key_code == KeyS && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == KeyLeft || (evt.key_code == KeyA && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key_code == KeyRight || (evt.key_code == KeyD && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
        } else if (evt.key_code == KeySpace) {
            wind_vector_goal_ = Ren::Vec3f{ 1.0f, 0.0f, 0.0f };
        } else if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
            shift_down_ = true;
        } else {
            input_processed = false;
        }
    }
    break;
    case RawInputEvent::EvKeyUp: {
        if (evt.key_code == KeyUp || (evt.key_code == KeyW && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == KeyDown || (evt.key_code == KeyS && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key_code == KeyLeft || (evt.key_code == KeyA && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else if (evt.key_code == KeyRight || (evt.key_code == KeyD && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else if (evt.key_code == KeySpace) {
            wind_vector_goal_ = Ren::Vec3f{ 0.0f };
        } else {
            input_processed = false;
        }
    }
    case RawInputEvent::EvResize:
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}

void GSDrawTest::OnUpdateScene() {
    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;

    // test test test
    TestUpdateAnims(delta_time_s);

    // Update camera
    scene_manager_->SetupView(
            view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f },
            view_fov_, max_exposure_);

    //log_->Info("%f %f %f | %f %f %f",
    //        view_origin_[0], view_origin_[1], view_origin_[2],
    //        view_dir_[0], view_dir_[1], view_dir_[2]);
}

void GSDrawTest::SaveScene(JsObject &js_scene) {
    GSBaseState::SaveScene(js_scene);

    {   // write camera
        JsObject js_camera;

        {   // write view origin
            JsArray js_view_origin;
            js_view_origin.Push(JsNumber{ (double)initial_view_origin_[0] });
            js_view_origin.Push(JsNumber{ (double)initial_view_origin_[1] });
            js_view_origin.Push(JsNumber{ (double)initial_view_origin_[2] });

            js_camera.Push("view_origin", std::move(js_view_origin));
        }

        {   // write view direction
            JsArray js_view_dir;
            js_view_dir.Push(JsNumber{ (double)initial_view_dir_[0] });
            js_view_dir.Push(JsNumber{ (double)initial_view_dir_[1] });
            js_view_dir.Push(JsNumber{ (double)initial_view_dir_[2] });

            js_camera.Push("view_dir", std::move(js_view_dir));
        }

        {   // write forward speed
            js_camera.Push("fwd_speed", JsNumber{ (double)max_fwd_speed_ });
        }

        {   // write fov
            js_camera.Push("fov", JsNumber{ (double)view_fov_ });
        }

        {   // write max exposure
            js_camera.Push("max_exposure", JsNumber{ (double)max_exposure_ });
        }

        js_scene.Push("camera", std::move(js_camera));
    }
}

void GSDrawTest::TestUpdateAnims(float delta_time_s) {
    SceneData &scene = scene_manager_->scene_data();

    if (wolf_indices_[0] != 0xffffffff) {
        for (uint32_t wolf_index : wolf_indices_) {
            if (wolf_index == 0xffffffff) break;

            SceneObject *wolf = scene_manager_->GetObject(wolf_index);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((wolf->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(wolf->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(wolf->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (font_meshes_[0] != 0xffffffff) {
        for (uint32_t font_mesh : font_meshes_) {
            SceneObject *font = scene_manager_->GetObject(font_mesh);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((font->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(font->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(font->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                skel->UpdateAnim(0, as->anim_time_s);
                skel->ApplyAnim(0);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (scooter_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 16; i++) {
            if (scooter_indices_[i] == 0xffffffff) break;

            SceneObject *scooter = scene_manager_->GetObject(scooter_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((scooter->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(scooter->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(scooter->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = (i < 8) ? 2 : 3;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (sophia_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 2; i++) {
            if (sophia_indices_[i] == 0xffffffff) break;

            SceneObject *sophia = scene_manager_->GetObject(sophia_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((sophia->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(sophia->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(sophia->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = 0;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (eric_indices_[0] != 0xffffffff) {
        for (int i = 0; i < 2; i++) {
            if (eric_indices_[i] == 0xffffffff) break;

            SceneObject *eric = scene_manager_->GetObject(eric_indices_[i]);

            uint32_t mask = CompDrawableBit | CompAnimStateBit;
            if ((eric->comp_mask & mask) == mask) {
                auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(eric->components[CompDrawable]);
                auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(eric->components[CompAnimState]);

                as->anim_time_s += delta_time_s;

                Ren::Mesh *mesh = dr->mesh.get();
                Ren::Skeleton *skel = mesh->skel();

                const int anim_index = 0;

                skel->UpdateAnim(anim_index, as->anim_time_s);
                skel->ApplyAnim(anim_index);
                skel->UpdateBones(as->matr_palette);
            }
        }
    }

    if (zenith_index_ != 0xffffffff) {
        SceneObject *zenith = scene_manager_->GetObject(zenith_index_);

        uint32_t mask = CompDrawableBit | CompAnimStateBit;
        if ((zenith->comp_mask & mask) == mask) {
            auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(zenith->components[CompDrawable]);
            auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(zenith->components[CompAnimState]);

            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(as->matr_palette);
        }
    }

    if (palm_index_ != 0xffffffff) {
        SceneObject *palm = scene_manager_->GetObject(palm_index_);

        uint32_t mask = CompDrawableBit | CompAnimStateBit;
        if ((palm->comp_mask & mask) == mask) {
            auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(palm->components[CompDrawable]);
            auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(palm->components[CompAnimState]);

            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(as->matr_palette);
        }
    }

    /*if (leaf_tree_index_ != 0xffffffff) {
        SceneObject *leaf_tree = scene_manager_->GetObject(leaf_tree_index_);

        uint32_t mask = CompDrawableBit | CompTransformBit;
        if ((leaf_tree->comp_mask & mask) == mask) {
            auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(leaf_tree->components[CompTransform]);

            Ren::Mat4f rot_mat;
            rot_mat = Ren::Rotate(rot_mat, 1.0f * delta_time_s, Ren::Vec3f{ 0.0f, 1.0f, 0.0f });

            tr->mat = rot_mat * tr->mat;
            scene_manager_->InvalidateObjects(&leaf_tree_index_, 1, CompTransformBit);
        }
    }*/

    const auto wind_scroll_dir = Ren::Vec2f{ scene.env.wind_vec[0], scene.env.wind_vec[2] };
    scene.env.wind_scroll_lf = Ren::Fract(scene.env.wind_scroll_lf - (1.0f / 256.0f) * delta_time_s * wind_scroll_dir);
    scene.env.wind_scroll_hf = Ren::Fract(scene.env.wind_scroll_hf - (1.0f / 32.0f) * delta_time_s * wind_scroll_dir);
}
