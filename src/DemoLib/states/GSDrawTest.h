#pragma once

#include <Eng/GameState.h>
#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/MVec.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/SW/SW.h>

#include "../Scene/SceneData.h"

class GameBase;
class GameStateManager;
class FontStorage;
class Renderer;
class SceneManager;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSDrawTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;
    std::shared_ptr<Renderer> renderer_;
    std::shared_ptr<SceneManager> scene_manager_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    TimingInfo last_timings_;

    bool view_grabbed_ = false;
    Ren::Vec3f view_origin_ = { 0, 1, 0 },
               view_dir_ = { 0, 0, -1 };

    float forward_speed_ = 0, side_speed_ = 0;

    bool use_pt_ = false;
    bool invalidate_view_ = true;

    std::vector<std::string> cmdline_history_;
    bool cmdline_enabled_ = false;
    bool shift_down_ = false;
public:
    explicit GSDrawTest(GameBase *game);
    ~GSDrawTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};