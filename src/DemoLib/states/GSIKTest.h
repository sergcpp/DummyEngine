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

namespace GSIKTestInternal {
    struct Bone {
        Ren::Vec3f dir, rot_axis;
        float length, angle;
        float min_angle, max_angle;
        Ren::Vec3f cur_pos, cur_rot_axis;
    };
}

class GSIKTest : public GameState {
    GameBase *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Ren::Context> ctx_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;

    Ren::ProgramRef line_prog_;
    std::vector<GSIKTestInternal::Bone> bones_;

    Ren::Camera cam_;

    bool view_grabbed_ = false;
    Ren::Vec3f cur_goal_, prev_goal_, next_goal_;

    float view_angle_ = 0.0f;

    int goal_change_timer_ = 0;
    int iterations_ = 0;
    float error_ = 0.0f;

    void UpdateBones();
public:
    explicit GSIKTest(GameBase *game);
    ~GSIKTest();

    void Enter() override;
    void Exit() override;

    void Draw(float dt_s) override;

    void Update(int dt_ms) override;

    void HandleInput(InputManager::Event) override;
};