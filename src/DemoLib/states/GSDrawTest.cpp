#include "GSDrawTest.h"

#include <fstream>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../Managers/SceneManager.h"
#include "../ui/FontStorage.h"

namespace GSDrawTestInternal {
    const float FORWARD_SPEED = 0.25f;

    const float CAM_FOV = 45.0f;
    const Ren::Vec3f CAM_CENTER = { 100.0f, 100.0f, -500.0f };
    const Ren::Vec3f CAM_TARGET = { 0.0f, 0.0f, 0.0f };
    const Ren::Vec3f CAM_UP = { 0.0f, 1.0f, 0.0f };

    
}

GSDrawTest::GSDrawTest(GameBase *game) : game_(game) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    renderer_       = game->GetComponent<Renderer>(RENDERER_KEY);
    scene_manager_  = game->GetComponent<SceneManager>(SCENE_MANAGER_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");
}

GSDrawTest::~GSDrawTest() {
    
}

void GSDrawTest::Enter() {
    using namespace GSDrawTestInternal;

    auto on_load_program = [&](const char *name, const char *arg1, const char *arg2) {
        std::string vs_name = "assets/shaders/";
        vs_name += arg1;
        std::string fs_name = "assets/shaders/";
        fs_name += arg2;

        Sys::AssetFile vs_file(vs_name, Sys::AssetFile::IN);
        size_t vs_file_size = vs_file.size();

        std::string vs_file_data;
        vs_file_data.resize(vs_file_size);
        vs_file.Read(&vs_file_data[0], vs_file_size);

        Sys::AssetFile fs_file(fs_name, Sys::AssetFile::IN);
        size_t fs_file_size = fs_file.size();

        std::string fs_file_data;
        fs_file_data.resize(fs_file_size);
        fs_file.Read(&fs_file_data[0], fs_file_size);

        return ctx_->LoadProgramGLSL(name, &vs_file_data[0], &fs_file_data[0], nullptr);
    };

    auto on_load_texture = [&](const char *name) {
        Sys::AssetFile in_file(name, Sys::AssetFile::IN);
        size_t in_file_size = in_file.size();

        std::unique_ptr<char[]> in_file_data(new char[in_file_size]);
        in_file.Read(&in_file_data[0], in_file_size);

        Ren::Texture2DParams p;

        return ctx_->LoadTexture2D(name, &in_file_data[0], (int)in_file_size, p, nullptr);
    };

    

    std::ifstream in_scene("assets/scenes/jap_house.json", std::ios::binary);

    JsObject js_scene;
    if (!js_scene.Read(in_scene)) {
        throw std::runtime_error("Cannot load scene!");
    }

    scene_manager_->LoadScene(js_scene);
}

void GSDrawTest::Exit() {

}

void GSDrawTest::Draw(float dt_s) {
    using namespace GSDrawTestInternal;

    {
        scene_manager_->SetupView(view_origin_, (view_origin_ + view_dir_), Ren::Vec3f{ 0.0f, 1.0f, 0.0f });
        scene_manager_->Draw();
    }

    {
        const auto timings = scene_manager_->timings();
        const auto back_timings = scene_manager_->back_timings();

        auto start = std::min(last_timings_.first, back_timings.first);
        auto end = std::max(last_timings_.second, back_timings.second);

        auto dur1 = std::chrono::duration_cast<std::chrono::microseconds>(last_timings_.second - last_timings_.first);
        auto dur2 = std::chrono::duration_cast<std::chrono::microseconds>(back_timings.second - back_timings.first);

        LOGI("Frontend: %lld\tBackend: %lld", (long long)dur2.count(), (long long)dur1.count());

        last_timings_ = timings;
    }

    {
        // ui draw
        ui_renderer_->BeginDraw();

        //font_->DrawText(ui_renderer_.get(), "111", { -1, 1.0f - 1 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s2.c_str(), { -1, 1.0f - 2 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s3.c_str(), { -1, 1.0f - 3 * font_->height(ui_root_.get()) }, ui_root_.get());
        //font_->DrawText(ui_renderer_.get(), s4.c_str(), { -1, 1.0f - 4 * font_->height(ui_root_.get()) }, ui_root_.get());

        ui_renderer_->EndDraw();
    }

    ctx_->ProcessTasks();
}

void GSDrawTest::Update(int dt_ms) {
    Ren::Vec3f up = { 0, 1, 0 };
    Ren::Vec3f side = Normalize(Cross(view_dir_, up));

    view_origin_ += view_dir_ * forward_speed_;
    view_origin_ += side * side_speed_;
}

void GSDrawTest::HandleInput(InputManager::Event evt) {
    using namespace Ren;
    using namespace GSDrawTestInternal;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        view_grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
         if (view_grabbed_) {
            Vec3f up = { 0, 1, 0 };
            Vec3f side = Normalize(Cross(view_dir_, up));
            up = Cross(side, view_dir_);

            Mat4f rot;
            rot = Rotate(rot, 0.01f * evt.move.dx, up);
            rot = Rotate(rot, 0.01f * evt.move.dy, side);

            auto rot_m3 = Mat3f(rot);
            view_dir_ = view_dir_ * rot_m3;
        }
        break;
    case InputManager::RAW_INPUT_KEY_DOWN: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = -FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = FORWARD_SPEED;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_SPACE) {

        }
    }
                                           break;
    case InputManager::RAW_INPUT_KEY_UP: {
        if (evt.key == InputManager::RAW_INPUT_BUTTON_UP) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_DOWN) {
            forward_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_LEFT) {
            side_speed_ = 0;
        } else if (evt.key == InputManager::RAW_INPUT_BUTTON_RIGHT) {
            side_speed_ = 0;
        }
    }
    case InputManager::RAW_INPUT_RESIZE:

        break;
    default:
        break;
    }
}
