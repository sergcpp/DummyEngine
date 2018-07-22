#pragma once

#include <engine/GameState.h>
#include <ren/Camera.h>
#include <ren/Program.h>
#include <ren/Texture.h>
#include <ren/SW/SW.h>

class GameBase;
class GameStateManager;
class FontStorage;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

struct image_t {
    Ren::eTex2DFormat format;
    int w, h;
    std::unique_ptr<uint8_t[]> data;
};

class GSBicubicTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    image_t orig_image_;

    void OnMouse(int x, int y);

public:
    explicit GSBicubicTest(GameBase *game);
    ~GSBicubicTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};