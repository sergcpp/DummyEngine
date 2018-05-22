#pragma once

#include <engine/GameState.h>
#include <ren/Camera.h>
#include <ren/Mesh.h>
#include <ren/Program.h>
#include <ren/Texture.h>
#include <ren/SW/SW.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace ui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSDefTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;

    std::shared_ptr<ui::Renderer> ui_renderer_;
    std::shared_ptr<ui::BaseElement> ui_root_;
    std::shared_ptr<ui::BitmapFont> font_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    math::vec3 view_origin_ = { 0, 20, 3 },
               view_dir_ = { 0, 0, 1 },
               view_target_ = { 0, 0, 0 };

    float forward_speed_ = 0, side_speed_ = 0;

    ren::Camera cam_;

    ren::MeshRef test_mesh_;

public:
    explicit GSDefTest(GameBase *game);
    ~GSDefTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};