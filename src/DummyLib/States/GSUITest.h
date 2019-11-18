#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Gui/BitmapFont.h>
#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/MVec.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/SW/SW.h>

#include "GSBaseState.h"

class Cmdline;
class DebugInfoUI;
class GameStateManager;
class FontStorage;
class SceneManager;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSUITest : public GSBaseState {
    int view_pointer_ = 0, move_pointer_ = 0;
    Ren::Vec3f view_origin_ = { 0, 1, 0 },
               view_dir_ = { 0, 0, -1 };

    float fwd_press_speed_ = 0, side_press_speed_ = 0,
          fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60.0f;

    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint32_t click_time_ = 0;

    Gui::BitmapFont test_font_;
    int test_string_length_ = 0;
    int test_progress_ = 0;
    float test_time_counter_s = 0.0f;

    void OnPostloadScene(JsObject &js_scene) override;

    void OnUpdateScene() override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;
public:
    explicit GSUITest(GameBase *game);
    ~GSUITest() final = default;

    void Enter() override;
    void Exit() override;

    bool HandleInput(const InputManager::Event &evt) override;
};