#include "GSDefTest.h"

#include <fstream>

#include <engine/GameStateManager.h>
#include <ren/Context.h>
#include <ren/GL.h>
#include <ren/Utils.h>
#include <sys/AssetFile.h>
#include <sys/Time_.h>
#include <ui/Renderer.h>

#include "../Viewer.h"
#include "../ui/FontStorage.h"

namespace GSDefTestInternal {
    const float FORWARD_SPEED = 0.25f;

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
    ctx_            = game->GetComponent<ren::Context>(REN_CONTEXT_KEY);

    ui_renderer_    = game->GetComponent<ui::Renderer>(UI_RENDERER_KEY);
    ui_root_        = game->GetComponent<ui::BaseElement>(UI_ROOT_KEY);

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

        sys::AssetFile vs_file(vs_name, sys::AssetFile::IN);
        size_t vs_file_size = vs_file.size();

        std::string vs_file_data;
        vs_file_data.resize(vs_file_size);
        vs_file.Read(&vs_file_data[0], vs_file_size);

        sys::AssetFile fs_file(fs_name, sys::AssetFile::IN);
        size_t fs_file_size = fs_file.size();

        std::string fs_file_data;
        fs_file_data.resize(fs_file_size);
        fs_file.Read(&fs_file_data[0], fs_file_size);

        return ctx_->LoadProgramGLSL(name, &vs_file_data[0], &fs_file_data[0], nullptr);
    };

    auto on_load_texture = [&](const char *name) {
        sys::AssetFile in_file(name, sys::AssetFile::IN);
        size_t in_file_size = in_file.size();

        std::unique_ptr<char[]> in_file_data(new char[in_file_size]);
        in_file.Read(&in_file_data[0], in_file_size);

        ren::Texture2DParams p;

        return ctx_->LoadTexture2D(name, &in_file_data[0], in_file_size, p, nullptr);
    };

    auto on_load_material = [&](const char *name) {
        std::string material_path = "assets/materials/";
        material_path += name;

        sys::AssetFile in_file(material_path.c_str(), sys::AssetFile::IN);
        size_t in_file_size = in_file.size();

        std::string in_file_data;
        in_file_data.resize(in_file_size);
        in_file.Read(&in_file_data[0], in_file_size);

        return ctx_->LoadMaterial(name, &in_file_data[0], nullptr, on_load_program, on_load_texture);
    };

    sys::AssetFile in_file("test.mesh", sys::AssetFile::IN);
    size_t in_file_size = in_file.size();

    std::unique_ptr<char[]> in_file_data(new char[in_file_size]);
    in_file.Read(&in_file_data[0], in_file_size);

    test_mesh_ = ctx_->LoadMesh("test_mesh", &in_file_data[0], on_load_material);

    InitInternal(game_->width, game_->height);

    for (int i = 0; i < 10000; i++) {
        lights_.emplace_back();

        auto &l = lights_.back();

        l.pos = math::vec3{ 30, 0, -5 };
        l.pos.x += 30.0f * float(rand()) / RAND_MAX;
        l.pos.y += 40.0f * float(rand()) / RAND_MAX;
        l.pos.z += 10.0f * float(rand()) / RAND_MAX;

        l.col = { float(rand()) / RAND_MAX, float(rand()) / RAND_MAX, float(rand()) / RAND_MAX };
        float m = std::max(std::max(l.col.x, l.col.y), l.col.z);
        l.col /= m;
        l.col *= 0.5f;
    }
}

void GSDefTest::Exit() {

}

void GSDefTest::Draw(float dt_s) {
    using namespace GSDefTestInternal;

    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glDisable(GL_DEPTH_TEST);

        DrawInternal();
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

    anim_time_ += dt_ms / 1000.0;

    float f_time = (float)anim_time_;

    for (auto &l : lights_) {
        //l.pos.x += 0.01f * std::sin(f_time);
        l.pos.y += 0.025f * std::sin(1.0f * f_time);
        //l.pos.z += 0.01f * std::sin(f_time);
    }
}

void GSDefTest::HandleInput(InputManager::Event evt) {
    using namespace GSDefTestInternal;
    using namespace math;

    switch (evt.type) {
    case InputManager::RAW_INPUT_P1_DOWN:
        view_grabbed_ = true;
        break;
    case InputManager::RAW_INPUT_P1_UP:
        view_grabbed_ = false;
        break;
    case InputManager::RAW_INPUT_P1_MOVE:
         if (view_grabbed_) {
            vec3 up = { 0, 1, 0 };
            vec3 side = normalize(cross(view_dir_, up));
            up = cross(side, view_dir_);

            mat4 rot;
            rot = rotate(rot, 0.01f * evt.move.dx, up);
            rot = rotate(rot, 0.01f * evt.move.dy, side);

            mat3 rot_m3 = mat3(rot);

            if (!view_targeted_) {
                view_dir_ = view_dir_ * rot_m3;
            } else {
                vec3 dir = view_origin_ - view_target_;
                dir = dir * rot_m3;
                view_origin_ = view_target_ + dir;
                view_dir_ = normalize(-dir);
            }
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
