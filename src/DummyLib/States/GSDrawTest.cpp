#include "GSDrawTest.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/DebugInfoUI.h"
#include "../Gui/FontStorage.h"
#include "../Viewer.h"
#include "../Renderer/Renderer.h"
#include "../Scene/SceneManager.h"
#include "../Utils/Cmdline.h"

namespace GSDrawTestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
    const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
        //"font_test.json";
        //"skin_test.json";
        "living_room_gumroad.json";
        //"bistro.json";
        //"pbr_test.json";
}

GSDrawTest::GSDrawTest(GameBase *game) : GSBaseState(game) {
}

void GSDrawTest::Enter() {
    using namespace GSDrawTestInternal;

    GSBaseState::Enter();

    LOGI("GSDrawTest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);
}

void GSDrawTest::OnPostloadScene(JsObject &js_scene) {
    GSBaseState::OnPostloadScene(js_scene);

    cam_follow_path_.clear();
    cam_follow_point_ = 0;
    cam_follow_param_ = 0.0f;

    if (js_scene.Has("camera")) {
        const JsObject &js_cam = (const JsObject &)js_scene.at("camera");
        if (js_cam.Has("view_origin")) {
            const JsArray &js_orig = (const JsArray &)js_cam.at("view_origin");
            view_origin_[0] = (float)((const JsNumber &)js_orig.at(0)).val;
            view_origin_[1] = (float)((const JsNumber &)js_orig.at(1)).val;
            view_origin_[2] = (float)((const JsNumber &)js_orig.at(2)).val;
        }

        if (js_cam.Has("view_dir")) {
            const JsArray &js_dir = (const JsArray &)js_cam.at("view_dir");
            view_dir_[0] = (float)((const JsNumber &)js_dir.at(0)).val;
            view_dir_[1] = (float)((const JsNumber &)js_dir.at(1)).val;
            view_dir_[2] = (float)((const JsNumber &)js_dir.at(2)).val;
        }

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = (float)js_fwd_speed.val;
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = (const JsNumber &)js_cam.at("fov");
            view_fov_ = (float)js_fov.val;
        }

        if (js_cam.Has("follow_path")) {
            const JsArray &js_points = (const JsArray &)js_cam.at("follow_path");
            for (const JsElement &el : js_points.elements) {
                const auto &js_point = static_cast<const JsArray &>(el);

                const auto &x = (const JsNumber &)js_point.at(0),
                    &y = (const JsNumber &)js_point.at(1),
                    &z = (const JsNumber &)js_point.at(2);

                cam_follow_path_.emplace_back((float)x.val, (float)y.val, (float)z.val);
            }
        }
    }

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
        up = { 0, 1, 0 },
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
        const Ren::Vec3f rot_center = { 44.5799f, 0.15f, 24.7763f };

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
                    tr->mat = Ren::Rotate(tr->mat, scooters_angle_ + i * 0.25f * Ren::Pi<float>(), { 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, { 6.5f, 0.0f, 0.0f });
                } else {
                    // outer circle
                    tr->mat = Ren::Rotate(tr->mat, -scooters_angle_ + (i - 8) * 0.25f * Ren::Pi<float>(), { 0.0f, 1.0f, 0.0f });
                    tr->mat = Ren::Translate(tr->mat, { -8.5f, 0.0f, 0.0f });
                }
            }
        }

        scene_manager_->InvalidateObjects(scooter_indices_, 16, CompTransformBit);
    }
}

bool GSDrawTest::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSDrawTestInternal;

    // pt switch for touch controls
    if (evt.type == InputManager::RAW_INPUT_P1_DOWN || evt.type == InputManager::RAW_INPUT_P2_DOWN) {
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
    case InputManager::RAW_INPUT_P1_DOWN:
        if (evt.point.x < ((float)ctx_->w() / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 1;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 1;
        }
        break;
    case InputManager::RAW_INPUT_P2_DOWN:
        if (evt.point.x < ((float)ctx_->w() / 3.0f) && move_pointer_ == 0) {
            move_pointer_ = 2;
        } else if (view_pointer_ == 0) {
            view_pointer_ = 2;
        }
        break;
    case InputManager::RAW_INPUT_P1_UP:
        if (move_pointer_ == 1) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 1) {
            view_pointer_ = 0;
        }
        break;
    case InputManager::RAW_INPUT_P2_UP:
        if (move_pointer_ == 2) {
            move_pointer_ = 0;
            fwd_touch_speed_ = 0;
            side_touch_speed_ = 0;
        } else if (view_pointer_ == 2) {
            view_pointer_ = 0;
        }
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        if (move_pointer_ == 1) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 1) {
            Vec3f up = { 0, 1, 0 };
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
    case InputManager::RAW_INPUT_P2_MOVE:
        if (move_pointer_ == 2) {
            side_touch_speed_ += evt.move.dx * 0.002f;
            side_touch_speed_ = std::max(std::min(side_touch_speed_, max_fwd_speed_), -max_fwd_speed_);

            fwd_touch_speed_ -= evt.move.dy * 0.002f;
            fwd_touch_speed_ = std::max(std::min(fwd_touch_speed_, max_fwd_speed_), -max_fwd_speed_);
        } else if (view_pointer_ == 2) {
            Vec3f up = { 0, 1, 0 };
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
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || (evt.raw_key == 'w' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || (evt.raw_key == 's' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || (evt.raw_key == 'a' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = -max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || (evt.raw_key == 'd' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = max_fwd_speed_;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {
            
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SHIFT) {
            shift_down_ = true;
        } else {
            input_processed = false;
        }
    }
    break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || (evt.raw_key == 'w' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN || (evt.raw_key == 's' && (!cmdline_enabled_ || view_pointer_))) {
            fwd_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT || (evt.raw_key == 'a' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT || (evt.raw_key == 'd' && (!cmdline_enabled_ || view_pointer_))) {
            side_press_speed_ = 0;
        } else {
            input_processed = false;
        }
    }
    case InputManager::RAW_INPUT_RESIZE:
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
    float delta_time_s = fr_info_.delta_time_us * 0.000001f;

    // test test test
    TestUpdateAnims(delta_time_s);

    // Update camera
    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
}

void GSDrawTest::TestUpdateAnims(float delta_time_s) {
    const SceneData &scene = scene_manager_->scene_data();

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
}