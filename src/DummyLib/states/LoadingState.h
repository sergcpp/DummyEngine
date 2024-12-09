#pragma once

#include <future>

#include <Eng/ViewerState.h>
#include <Eng/utils/FrameInfo.h>
#include <Ren/RastState.h>
#include <Ren/TextureRegion.h>
#include <Sys/Json.h>
#include <Sys/PoolAlloc.h>

class Viewer;
namespace Eng {
class ILog;
class ShaderLoader;
class PrimDraw;
}; // namespace Eng
namespace Gui {
class BaseElement;
class BitmapFont;
class Renderer;
} // namespace Gui
namespace Ren {
class Context;
}
namespace Sys {
class ThreadPool;
}

class LoadingState : public Eng::ViewerState {
    Viewer *viewer_;
    Ren::Context *ren_ctx_ = nullptr;
    Eng::ILog *log_ = nullptr;
    Eng::ShaderLoader *sh_loader_ = nullptr;
    Eng::PrimDraw *prim_draw_ = nullptr;
    Sys::ThreadPool *threads_ = nullptr;
    Gui::Renderer *ui_renderer_ = nullptr;
    Gui::BaseElement *ui_root_ = nullptr;
    const Gui::BitmapFont *font_ = nullptr;

    Ren::TextureRegionRef dummy_white_;

    Ren::ProgramRef blit_loading_prog_;
    Ren::RastState curr_rast_state_;

    Eng::FrameInfo fr_info_;
    uint64_t loading_start_ = 0;

    Sys::MultiPoolAllocator<char> alloc_;
    Sys::JsArrayP pipelines_to_init_;
    int pipeline_index_ = 0;
    std::vector<std::future<void>> futures_;

    void InitPipelines(const Sys::JsArrayP &js_pipelines, int start, int count);

  public:
    explicit LoadingState(Viewer *viewer);
    ~LoadingState() override;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};