#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Config.h"
#include "FrameInfo.h"

namespace Gui {
class Renderer;
class RootElement;
}

namespace Ren {
class Context;
}

namespace Snd {
class Context;
}

namespace Sys {
class ThreadPool;
}

namespace Eng {
class Cmdline;
class FlowControl;
class ViewerStateManager;
class ILog;
class InputManager;
class PhysicsManager;
class Random;
class Renderer;
class SceneManager;
class ShaderLoader;
struct TimeInterval {
    uint64_t start_timepoint_us = 0, end_timepoint_us = 0;
};
class ViewerBase {
  protected:
    ILog *log_;
    std::unique_ptr<Ren::Context> ren_ctx_;
    std::unique_ptr<Snd::Context> snd_ctx_;
    std::unique_ptr<Sys::ThreadPool> threads_;
    std::unique_ptr<InputManager> input_manager_;
    std::unique_ptr<ShaderLoader> shader_loader_;
    std::unique_ptr<FlowControl> flow_control_;
    std::unique_ptr<Random> random_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Cmdline> cmdline_;
    std::unique_ptr<PhysicsManager> physics_manager_;
    std::unique_ptr<SceneManager> scene_manager_;
    std::unique_ptr<ViewerStateManager> state_manager_;
    std::unique_ptr<Gui::Renderer> ui_renderer_;
    std::unique_ptr<Gui::RootElement> ui_root_;
    Eng::FrameInfo fr_info_;

    void InitOptickGPUProfiler();

  public:
    ViewerBase(int w, int h, int validation_level, bool nohwrt, ILog *log, std::string_view device_name);
    virtual ~ViewerBase();

    ILog *log() { return log_; }
    Ren::Context *ren_ctx() { return ren_ctx_.get(); }
    Snd::Context *snd_ctx() { return snd_ctx_.get(); }
    Sys::ThreadPool *threads() { return threads_.get(); }
    InputManager *input_manager() { return input_manager_.get(); }
    ShaderLoader *shader_loader() { return shader_loader_.get(); }
    FlowControl *flow_control() { return flow_control_.get(); }
    Random *random() { return random_.get(); }
    Renderer *renderer() { return renderer_.get(); }
    Cmdline *cmdline() { return cmdline_.get(); }
    PhysicsManager *physics_manager() { return physics_manager_.get(); }
    SceneManager *scene_manager() { return scene_manager_.get(); }
    ViewerStateManager *state_manager() { return state_manager_.get(); }
    Gui::Renderer *ui_renderer() { return ui_renderer_.get(); }
    Gui::RootElement *ui_root() { return ui_root_.get(); }

    virtual void Resize(int w, int h);

    virtual void Start();
    virtual void Frame();
    virtual void Quit();

    std::atomic_bool terminated;
    int width, height;
};
} // namespace Eng