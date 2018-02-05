#pragma once

#include <engine/GameState.h>
#include <engine/go/Go.h>
#include <ren/Camera.h>
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

class GSOccTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<ren::Context> ctx_;

    std::shared_ptr<ui::Renderer> ui_renderer_;
    std::shared_ptr<ui::BaseElement> ui_root_;
    std::shared_ptr<ui::BitmapFont> font_;

    SWcull_ctx cull_ctx_;

    bool view_grabbed_ = false;
    bool view_targeted_ = false;
    math::vec3 view_origin_ = { 0, 20, 3 },
               view_dir_ = { -1, 0, 0 },
               view_target_ = { 0, 0, 0 };

    math::vec3 sun_dir_ = { 0, 1, 0 };

    bool invalidate_preview_ = true;

    float forward_speed_ = 0, side_speed_ = 0;

	float time_acc_ = 0;
	int fps_counter_ = 0;

#if defined(USE_GL_RENDER)
	ren::ProgramRef main_prog_;
#endif

	void InitShaders();
	void DrawBBox(const float min[3], const float max[3]);

public:
    explicit GSOccTest(GameBase *game);
	~GSOccTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};