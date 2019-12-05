#include "GSUITest.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Gui/Image.h>
#include <Gui/ImageNinePatch.h>
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

#include "../Gui/FontStorage.h"
#include "../Gui/TextPrinter.h"
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
    u8"<violet>You know, being Caroline taught me a</violet> <cyan>valueable</cyan> <violet>lesson. "
    u8"I</violet> <red>♥</red> <white>thought</white> <red>♥</red> <violet>you were my greatest enemy, when all along you "
    u8"were my best friend.</violet> <yellow>The surge of emotion that shout through "
    u8"me when I saved your life taught me an even more valuable lesson</yellow> <white>-</white> "
    u8"<cyan>where caroline lives in my brain.</cyan>";

static const char test_string_de[] =
    u8"<violet>Als Caroline lernte ich eine</violet> <cyan>wichtige</cyan> <violet>Lektion. Ich</violet> <red>♥</red><white>dachte</white><red>♥</red>, "
    u8"<violet>du wärst mein größter Feind und warst doch die ganze Zeit mein bester Freund.</violet>"
    //u8" Als Caroline lernte ich eine weitere wichtige Lektion: Wo Caroline in meinem Hirn lebt."
    u8" <yellow>Die Emotionen, die ich verspürte, als ich dein Leben rettete, erteilten mir eine noch viel wichtigere Lektion:</yellow> <cyan>Ich weiß jetzt, wo Caroline in meinem Gehirn lebt."
    u8" Leb wohl, Caroline.</cyan>";

static const char *test_string = test_string_de;
}

GSUITest::GSUITest(GameBase *game) : GSBaseState(game) {
    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    dialog_font_ = fonts->FindFont("dialog_font");
    dialog_font_->set_scale(1.5f);
}

void GSUITest::Enter() {
    using namespace GSUITestInternal;

    GSBaseState::Enter();

    LOGI("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    test_image_.reset(new Gui::Image{
        *ctx_, "assets_pc/textures/test_image.uncompressed.png", Ren::Vec2f{ -0.5f, -0.5f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    test_frame_.reset(new Gui::ImageNinePatch{
        *ctx_, "assets_pc/textures/ui/frame_01.uncompressed.png", Ren::Vec2f{ 2.0f, 2.0f }, 1.0f, Ren::Vec2f{ 0.0f, 0.1f }, Ren::Vec2f{ 0.5f, 0.5f }, ui_root_.get()
    });

    text_printer_.reset(new TextPrinter{
        *ctx_, Ren::Vec2f{ -0.995f, -0.995f }, Ren::Vec2f{ 1.99f, 1.1f }, ui_root_.get(), dialog_font_
    });

    const char *dialog_name = "assets/scenes/test/test_dialog.json";
    JsObject js_script;

    {   // Load dialog data from file
        Sys::AssetFile in_scene(dialog_name);
        if (!in_scene) {
            LOGE("Can not open dialog file %s", dialog_name);
        }

        size_t scene_size = in_scene.size();

        std::unique_ptr<uint8_t[]> scene_data(new uint8_t[scene_size]);
        in_scene.Read((char *)&scene_data[0], scene_size);

        Sys::MemBuf mem(&scene_data[0], scene_size);
        std::istream in_stream(&mem);

        if (!js_script.Read(in_stream)) {
            throw std::runtime_error("Cannot load dialog!");
        }
    }

    text_printer_->LoadScript(js_script);
}

void GSUITest::OnPostloadScene(JsObject &js_scene) {
    using namespace GSUITestInternal;

    GSBaseState::OnPostloadScene(js_scene);

    /*if (js_scene.Has("camera")) {
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
    }*/

    //scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f }, view_fov_);
}

void GSUITest::OnUpdateScene() {
    using namespace GSUITestInternal;

    GSBaseState::OnUpdateScene();

    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;
    test_time_counter_s += delta_time_s;

    const float char_period_s = 0.025f;

    while (test_time_counter_s > char_period_s) {
        text_printer_->incr_progress();
        test_time_counter_s -= char_period_s;
    }
}

void GSUITest::Exit() {
    GSBaseState::Exit();
}

void GSUITest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITestInternal;

    GSBaseState::DrawUI(r, root);

    dialog_font_->set_scale(std::max(root->size_px()[0] / 1024.0f, 1.0f));

    text_printer_->Draw(r);

    //test_image_->Draw(r);
    //test_frame_->Draw(r);
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
    case InputManager::RAW_INPUT_P1_DOWN: {
        Ren::Vec2f p = Gui::MapPointToScreen({ (int)evt.point.x, (int)evt.point.y }, { ctx_->w(), ctx_->h() });
        text_printer_->Press(p, true);
    } break;
    case InputManager::RAW_INPUT_P2_DOWN: {
        
    } break;
    case InputManager::RAW_INPUT_P1_UP: {
        text_printer_->skip();

        Ren::Vec2f p = Gui::MapPointToScreen({ (int)evt.point.x, (int)evt.point.y }, { ctx_->w(), ctx_->h() });
        text_printer_->Press(p, false);
    } break;
    case InputManager::RAW_INPUT_P2_UP: {

    } break;
    case InputManager::RAW_INPUT_P1_MOVE: {
        Ren::Vec2f p = Gui::MapPointToScreen({ (int)evt.point.x, (int)evt.point.y }, { ctx_->w(), ctx_->h() });
        text_printer_->Focus(p);
    } break;
    case InputManager::RAW_INPUT_P2_MOVE: {

    } break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        input_processed = false;
    } break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP || (evt.raw_key == 'w' && !cmdline_enabled_)) {
            text_printer_->restart();
        } else {
            input_processed = false;
        }
    } break;
    case InputManager::RAW_INPUT_RESIZE:
        text_printer_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}
