#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

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
} // namespace Gui

class GSPhyTest final : public GSBaseState {
    int view_pointer_ = 0, move_pointer_ = 0;

    Ren::Vec3f initial_view_pos_ = Ren::Vec3f{0, 1, 0}, initial_view_dir_ = Ren::Vec3f{0, 0, -1};

    Ren::Vec3f view_origin_, view_dir_;

    float fwd_press_speed_ = 0, side_press_speed_ = 0, fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60.0f;
    float max_exposure_ = 1000.0f;

    uint64_t click_time_ = 0;

    void OnPreloadScene(JsObjectP &js_scene) override;
    void OnPostloadScene(JsObjectP &js_scene) override;

    void SaveScene(JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

  public:
    explicit GSPhyTest(Eng::GameBase *game);
    ~GSPhyTest() final = default;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::InputManager::Event &evt) override;
};