#include "GSDefTest.h"

#include <fstream>

#include <Eng/GameStateManager.h>
#include <Gui/Renderer.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSDefTestInternal {
    const float FORWARD_SPEED = 2.0f;

    const float CAM_FOV = 45.0f;
    const float CAM_CENTER[3] = { 100.0f, 100.0f, -500.0f };
    const float CAM_TARGET[3] = { 0.0f, 0.0f, 0.0f };
    const float CAM_UP[3] = { 0.0f, 1.0f, 0.0f };

    const float NEAR_CLIP = 0.5f;
    const float FAR_CLIP = 1000;
}

GSDefTest::GSDefTest(GameBase *game) : game_(game),
    cam_(GSDefTestInternal::CAM_CENTER,
         GSDefTestInternal::CAM_TARGET,
         GSDefTestInternal::CAM_UP) {
    state_manager_  = game->GetComponent<GameStateManager>(STATE_MANAGER_KEY);
    ctx_            = game->GetComponent<Ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<Gui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<Gui::BaseElement>(UI_ROOT_KEY);

    const auto fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    font_ = fonts->FindFont("main_font");

    using namespace GSDefTestInternal;

    cam_.Perspective(CAM_FOV, float(game_->width) / game_->height, NEAR_CLIP, FAR_CLIP);
}

GSDefTest::~GSDefTest() {
    
}

void GSDefTest::Enter() {
    using namespace GSDefTestInternal;
    using namespace math;

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

        return ctx_->LoadTexture2D(name, &in_file_data[0], in_file_size, p, nullptr);
    };

    auto on_load_material = [&](const char *name) {
        std::string material_path = "assets/materials/";
        material_path += name;

        Sys::AssetFile in_file(material_path.c_str(), Sys::AssetFile::IN);
        size_t in_file_size = in_file.size();

        std::string in_file_data;
        in_file_data.resize(in_file_size);
        in_file.Read(&in_file_data[0], in_file_size);

        return ctx_->LoadMaterial(name, &in_file_data[0], nullptr, on_load_program, on_load_texture);
    };

    Sys::AssetFile in_file("test.mesh", Sys::AssetFile::IN);
    size_t in_file_size = in_file.size();

    std::unique_ptr<char[]> in_file_data(new char[in_file_size]);
    in_file.Read(&in_file_data[0], in_file_size);

    test_mesh_ = ctx_->LoadMesh("test_mesh", &in_file_data[0], on_load_material);
}

void GSDefTest::Exit() {

}

void GSDefTest::Draw(float dt_s) {
    using namespace GSDefTestInternal;

    {
        glClearColor(0, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        
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

void GSDefTest::Update(int dt_ms) {
    using namespace math;

    vec3 up = { 0, 1, 0 };
    vec3 side = normalize(cross(view_dir_, up));

    view_origin_ += view_dir_ * forward_speed_;
    view_origin_ += side * side_speed_;

    cam_.SetupView(value_ptr(view_origin_), value_ptr(view_origin_ + view_dir_), value_ptr(up));
}

void GSDefTest::HandleInput(InputManager::Event evt) {
    using namespace GSDefTestInternal;
    using namespace math;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        
        break;
    case InputManager::RAW_INPUT_P1_UP:
        
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
        
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
