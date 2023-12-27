#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneData.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

class DebugInfoUI;
class FontStorage;
namespace Eng {
class Cmdline;
class GameStateManager;
class PhysicsManager;
class SceneManager;
class ShaderLoader;
} // namespace Eng

namespace Gui {
class BaseElement;
class BitmapFont;
class Image9Patch;
class Renderer;
} // namespace Gui

namespace Snd {
class Context;
}

class GSBaseState : public Eng::GameState {
  protected:
    Eng::GameBase *game_;
    std::weak_ptr<Eng::GameStateManager> state_manager_;
    std::shared_ptr<Eng::Cmdline> cmdline_;
    std::shared_ptr<Ren::Context> ren_ctx_;
    std::shared_ptr<Snd::Context> snd_ctx_;
    std::shared_ptr<Ren::ILog> log_;
    std::shared_ptr<Eng::Renderer> renderer_;
    std::shared_ptr<Eng::SceneManager> scene_manager_;
    std::shared_ptr<Eng::PhysicsManager> physics_manager_;
    std::shared_ptr<Eng::TimeInterval> swap_interval_;
    std::shared_ptr<Eng::Random> random_;
    std::shared_ptr<Eng::ShaderLoader> shader_loader_;

    std::shared_ptr<Gui::Renderer> ui_renderer_;
    std::shared_ptr<Gui::BaseElement> ui_root_;
    std::shared_ptr<Gui::BitmapFont> font_;
    std::shared_ptr<DebugInfoUI> debug_ui_;
    std::unique_ptr<Gui::Image9Patch> cmdline_back_;

    std::mutex mtx_;
    std::thread background_thread_;
    std::condition_variable thr_notify_, thr_done_;
    bool shutdown_ = false, notified_ = false;

    // Enable all flags, Renderer will mask out what is not enabled
    uint64_t render_flags_ = 0xffffffffffffffff;

    Eng::FrameInfo fr_info_;

    Ren::Camera temp_probe_cam_;
    FrameBuf temp_probe_buf_;
    Eng::DrawList temp_probe_lists_[6];
    Eng::LightProbe *probe_to_render_ = nullptr, *probe_to_update_sh_ = nullptr;
    bool probes_dirty_ = true;
    int probe_sh_update_iteration_ = 0;
    std::vector<int> probes_to_update_;

    std::atomic_bool update_all_probes_{false};

    bool use_pt_ = false, use_lm_ = false;
    bool invalidate_view_ = true;

    Eng::DrawList main_view_lists_[2];
    int front_list_ = 0;

    Eng::FrontendInfo prev_front_info_;
    Eng::BackendInfo prev_back_info_;
    Eng::TimeInterval prev_swap_interval_;

    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    std::vector<Eng::InputManager::Event> cmdline_input_;
    std::vector<std::string> cmdline_history_;
    int cmdline_history_index_ = -1;
    uint64_t cmdline_cursor_blink_us_ = 0;
    bool cmdline_enabled_ = false;
    bool ui_enabled_ = true;
    bool shift_down_ = false;

    bool LoadScene(const char *name);

    virtual void OnPreloadScene(JsObjectP &js_scene);
    virtual void OnPostloadScene(JsObjectP &js_scene);

    virtual void SaveScene(JsObjectP &js_scene);

    void BackgroundProc();
    void UpdateFrame(int list_index);

    virtual void DrawUI(Gui::Renderer *r, Gui::BaseElement *root);

  public:
    explicit GSBaseState(Eng::GameBase *game);
    ~GSBaseState() override;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::InputManager::Event &evt) override;
};