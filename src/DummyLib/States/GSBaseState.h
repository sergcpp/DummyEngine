#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Eng/Scene/SceneData.h>
#include <Eng/Renderer/Renderer.h>
#include <Ren/Camera.h>
#include <Ren/Mesh.h>
#include <Ren/MVec.h>
#include <Ren/Program.h>
#include <Ren/Texture.h>
#include <Ren/SW/SW.h>

class Cmdline;
class DebugInfoUI;
class GameStateManager;
class FontStorage;
class SceneManager;
class Random;

namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
}

class GSBaseState : public GameState {
protected:
    GameBase                        *game_;
    std::weak_ptr<GameStateManager> state_manager_;
    std::shared_ptr<Cmdline>        cmdline_;
    std::shared_ptr<Ren::Context>   ctx_;
    std::shared_ptr<Ren::ILog>      log_;
    std::shared_ptr<Renderer>       renderer_;
    std::shared_ptr<SceneManager>   scene_manager_;
    std::shared_ptr<TimeInterval>   swap_interval_;
    std::shared_ptr<Random>         random_;

    std::shared_ptr<Gui::Renderer>      ui_renderer_;
    std::shared_ptr<Gui::BaseElement>   ui_root_;
    std::shared_ptr<Gui::BitmapFont>    font_;
    std::shared_ptr<DebugInfoUI>        debug_ui_;

    std::mutex              mtx_;
    std::thread             background_thread_;
    std::condition_variable thr_notify_, thr_done_;
    bool shutdown_ = false, notified_ = false;

    // Enable all flags, Renderer will mask out what is not enabled
    uint32_t                render_flags_ = 0xffffffff;

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

    bool use_pt_ = false, use_lm_ = false;
    bool invalidate_view_ = true;

    DrawList main_view_lists_[2];
    int front_list_ = 0;

    FrontendInfo prev_front_info_;
    BackendInfo prev_back_info_;
    TimeInterval prev_swap_interval_;

    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    std::vector<InputManager::Event>    cmdline_input_;
    std::vector<std::string>            cmdline_history_;
    bool                                cmdline_enabled_ = false;
    bool                                shift_down_ = false;

    bool LoadScene(const char *name);

    virtual void OnPreloadScene(JsObject &js_scene);
    virtual void OnPostloadScene(JsObject &js_scene);

    virtual void SaveScene(JsObject &js_scene);

    virtual void OnUpdateScene() {}

    void BackgroundProc();
    void UpdateFrame(int list_index);

    virtual void DrawUI(Gui::Renderer *r, Gui::BaseElement *root);
public:
    explicit GSBaseState(GameBase *game);
    ~GSBaseState() = default;

    void Enter() override;
    void Exit() override;

    void Draw(uint64_t dt_us) override;

    void Update(uint64_t dt_us) override;

    bool HandleInput(const InputManager::Event &evt) override;
};