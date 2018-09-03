#pragma once

#include <Eng/GameState.h>
#include <Ren/Camera.h>
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

class GSIKTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

public:
    explicit GSIKTest(GameBase *game);
    ~GSIKTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};