#pragma once

#include <Eng/GameState.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
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

class GSOccTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    Ren::Camera cam_;
    SWcull_ctx cull_ctx_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    Ren::Vec3f view_origin_ = { 0, 20, 3 },
               view_dir_ = { 0, 0, 1 },
               view_target_ = { 0, 0, 0 };

    bool invalidate_preview_ = true;

    float forward_speed_ = 0, side_speed_ = 0;

    float time_acc_ = 0;
    int fps_counter_ = 0;

    bool cull_ = true;
    bool wireframe_ = false;

#if defined(USE_GL_RENDER)
    Ren::ProgramRef main_prog_;
#endif

    void InitShaders();
    void DrawBoxes(SWcull_surf *surfs, int count);
    void DrawCam();
    void BlitDepthBuf();
    void BlitDepthTiles();

public:
    explicit GSOccTest(GameBase *game);
    ~GSOccTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};