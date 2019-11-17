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

#include "../Scene/SceneData.h"
#include "../Renderer/Renderer.h"

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

class GSDrawTest : public GameState {
    GameBase                        *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Cmdline>        cmdline_;
    std::shared_ptr<Ren::Context>   ctx_;
    std::shared_ptr<Renderer>       renderer_;
    std::shared_ptr<SceneManager>   scene_manager_;
    std::shared_ptr<TimeInterval>   swap_interval_;

    std::shared_ptr<Gui::Renderer>      ui_renderer_;
    std::shared_ptr<Gui::BaseElement>   ui_root_;
    std::shared_ptr<Gui::BitmapFont>    font_;
    std::shared_ptr<DebugInfoUI>        debug_ui_;

    std::mutex              mtx_;
    std::thread             background_thread_;
    std::condition_variable thr_notify_, thr_done_;
    bool shutdown_ = false, notified_ = false;

    FrameInfo fr_info_;

    Ren::Camera temp_probe_cam_;
    FrameBuf temp_probe_buf_;
    DrawList temp_probe_lists_[6];
    LightProbe *probe_to_render_ = nullptr,
               *probe_to_update_sh_ = nullptr;
    bool probes_dirty_ = true;
    int probe_sh_update_iteration_ = 0;
    std::vector<int> probes_to_update_;

    std::atomic_bool update_all_probes_{ false };

    int view_pointer_ = 0, move_pointer_ = 0;
    Ren::Vec3f view_origin_ = { 0, 1, 0 },
               view_dir_ = { 0, 0, -1 };

    float fwd_press_speed_ = 0, side_press_speed_ = 0,
          fwd_touch_speed_ = 0, side_touch_speed_ = 0;

    float max_fwd_speed_ = 0.5f, view_fov_ = 60.0f;

    bool use_pt_ = false, use_lm_ = false;
    bool invalidate_view_ = true;

    DrawList main_view_lists_[2];
    int front_list_ = 0;

    FrontendInfo prev_front_info_;
    BackendInfo prev_back_info_;
    TimeInterval prev_swap_interval_;

    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint32_t click_time_ = 0;

    std::vector<InputManager::Event>    cmdline_input_;
    std::vector<std::string>            cmdline_history_;
    bool                                cmdline_enabled_ = false;
    bool                                shift_down_ = false;

    // test test
    uint32_t wolf_indices_[32]      = { 0xffffffff };
    uint32_t scooter_indices_[16]   = { 0xffffffff };
    uint32_t sophia_indices_[2]     = { 0xffffffff }, eric_indices_[2] = { 0xffffffff };
    uint32_t font_meshes_[4]        = { 0xffffffff };
    float scooters_angle_ = 0.0f;

    std::vector<Ren::Vec3f>             cam_follow_path_;
    int                                 cam_follow_point_;
    float                               cam_follow_param_;

    void LoadScene(const char *name);

    void BackgroundProc();
    void UpdateFrame(int list_index);

    void TestUpdateAnims(float delta_time_s);
public:
    explicit GSDrawTest(GameBase *game);
    ~GSDrawTest();

    void Enter() override;
    void Exit() override;

    void Draw(uint64_t dt_us) override;

    void Update(uint64_t dt_us) override;

    void HandleInput(const InputManager::Event &evt) override;
};