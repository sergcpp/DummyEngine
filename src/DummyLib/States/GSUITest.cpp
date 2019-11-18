#include "GSUITest.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Gui/Utils.h>
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

namespace GSUITestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
    "empty.json";

static const char test_string_en[] =
    u8"You know, being Caroline taught me a valueable lesson."
    u8"I thought you were my greatest enemy, when all along you "
    u8"were my best friend. The surge of emotion that shout through "
    u8"me when I saved your life taught me an even more valuable lesson - "
    u8"where caroline lives in my brain.";

static const char test_string_de[] =
    u8"Als Caroline lernte ich eine wichtige Lektion. Ich DACHTE, du wärst mein größter Feind und warst doch die ganze Zeit mein bester Freund."
    u8" Als Caroline lernte ich eine weitere wichtige Lektion: Wo Caroline in meinem Hirn lebt."
    u8" Die Emotionen, die ich verspürte, als ich dein Leben rettete, erteilten mir eine noch viel wichtigere Lektion: Ich weiß jetzt, wo Caroline in meinem Gehirn lebt."
    u8" Leb wohl, Caroline.";
}

GSUITest::GSUITest(GameBase *game) : GSBaseState(game) {
}

void GSUITest::Enter() {
    using namespace GSUITestInternal;

    GSBaseState::Enter();

    LOGI("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);
}

void GSUITest::OnPostloadScene(JsObject &js_scene) {
    using namespace GSUITestInternal;

    GSBaseState::OnPostloadScene(js_scene);

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
    }

    scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);

    test_font_.Load("assets_pc/fonts/Roboto-Regular_48px_sdf.font", *ctx_);
    test_font_.set_scale(1.5f);

    int char_pos = 0;
    while (test_string_de[char_pos]) {
        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&test_string_de[char_pos], unicode);

        ++test_string_length_;
    }
}

void GSUITest::OnUpdateScene() {
    using namespace GSUITestInternal;

    GSBaseState::OnUpdateScene();

    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;
    test_time_counter_s += delta_time_s;

    const float char_period_s = 0.025f;

    while (test_time_counter_s > char_period_s) {
        test_progress_ = std::min(test_progress_ + 1, test_string_length_);
        test_time_counter_s -= char_period_s;
    }
}

void GSUITest::Exit() {
    GSBaseState::Exit();
}

void GSUITest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITestInternal;

    GSBaseState::DrawUI(r, root);

    const float font_height = test_font_.height(root);

    

    float y_offset = 1.0f - font_height;

    std::string line_string;

    int char_pos = 0, split_pos = 0, split_prog = 0;
    for (int i = 0; i < test_progress_; i++) {
        if (!test_string_de[char_pos]) break;
        const int char_start = char_pos;

        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&test_string_de[char_pos], unicode);

        for (int j = char_start; j < char_pos; j++) {
            line_string += test_string_de[j];
        }

        bool draw = false;

        if (unicode == Gui::g_unicode_spacebar) {
            split_pos = char_start;
            split_prog = i;

            int next_pos = char_pos;
            while (test_string_de[next_pos]) {
                const int next_start = next_pos;

                uint32_t unicode;
                next_pos += Gui::ConvChar_UTF8_to_Unicode(&test_string_de[next_pos], unicode);

                for (int j = next_start; j < next_pos; j++) {
                    line_string += test_string_de[j];
                }

                if (unicode == Gui::g_unicode_spacebar) break;
            }

            const float width = test_font_.GetWidth(line_string.c_str(), root);
            if (width > 2.0f) {
                char_pos = split_pos + 1;
                i = split_prog;

                draw = true;
            }

            const int n = next_pos - char_pos;
            line_string.erase(line_string.size() - n, n);
        }

        const float width = test_font_.GetWidth(line_string.c_str(), root);
        if (width > 2.0f) {
            const int n = char_pos - split_pos;
            line_string.erase(line_string.size() - n, n);
            char_pos = split_pos + 1;
            i = split_prog;

            draw = true;
        }

        if (draw) {
            test_font_.DrawText(r, line_string.c_str(), { -1.0f, y_offset }, root);
            line_string.clear();
            y_offset -= font_height;
        }
    }

    // draw the last line
    test_font_.DrawText(r, line_string.c_str(), { -1.0f, y_offset }, root);
}

bool GSUITest::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSUITestInternal;

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
            test_progress_ = 0;
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
