#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/ViewerBase.h>
#include <Eng/ViewerState.h>
#include <Gui/BaseElement.h>
#include <Gui/BitmapFont.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

#include "BaseState.h"

class DebugInfoUI;
class ViewerStateManager;
class FontStorage;
class SceneManager;
class WordPuzzleUI;

class UITest final : public BaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    const Gui::BitmapFont *dialog_font_ = {};
    float test_time_counter_s = 0;

    std::unique_ptr<Gui::Image> test_image_;
    std::unique_ptr<Gui::Image9Patch> test_frame_;
    std::unique_ptr<WordPuzzleUI> word_puzzle_;

    uint32_t sophia_indices_[2] = {0xffffffff};

    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void UpdateAnim(uint64_t dt_us) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

  public:
    explicit UITest(Viewer *viewer);
    ~UITest() final;

    void Enter() override;
    void Exit() override;
};