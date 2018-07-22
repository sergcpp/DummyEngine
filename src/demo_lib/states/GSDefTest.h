#pragma once

#include <Eng/GameState.h>
#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/SW/SW.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSDefTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    math::vec3 view_origin_ = { 0, 20, 3 },
               view_dir_ = { 0, 0, 1 },
               view_target_ = { 0, 0, 0 };

    float forward_speed_ = 0, side_speed_ = 0;

    Ren::Camera cam_;

    Ren::MeshRef test_mesh_;

public:
    explicit GSDefTest(GameBase *game);
    ~GSDefTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};