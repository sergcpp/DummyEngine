#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
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

class GSDrawTest : public GSBaseState {
    int view_pointer_ = 0, move_pointer_ = 0;
    Ren::Vec3f view_origin_ = { 0, 1, 0 },
               view_dir_ = { 0, 0, -1 };

    float fwd_press_speed_ = 0, side_press_speed_ = 0,
          fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60.0f;
    float max_exposure_ = 1000.0f;

    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint32_t click_time_ = 0;

    // test test
    uint32_t wolf_indices_[32]      = { 0xffffffff };
    uint32_t scooter_indices_[16]   = { 0xffffffff };
    uint32_t sophia_indices_[2]     = { 0xffffffff }, eric_indices_[2] = { 0xffffffff };
    uint32_t font_meshes_[4]        = { 0xffffffff };
    uint32_t zenith_index_          = 0xffffffff;
    float scooters_angle_ = 0.0f;

    std::vector<Ren::Vec3f>             cam_follow_path_;
    int                                 cam_follow_point_;
    float                               cam_follow_param_;

    void OnPostloadScene(JsObject &js_scene) override;

    void OnUpdateScene() override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void TestUpdateAnims(float delta_time_s);
public:
    explicit GSDrawTest(GameBase *game);
    ~GSDrawTest() final = default;

    void Enter() override;
    void Exit() override;

    void Draw(uint64_t dt_us) override;

    void Update(uint64_t dt_us) override;

    bool HandleInput(const InputManager::Event &evt) override;
};