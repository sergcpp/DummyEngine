#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Eng/gui/BaseElement.h>
#include <Eng/gui/BitmapFont.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

#include "GSBaseState.h"

class Cmdline;
class DebugInfoUI;
class GameStateManager;
class FontStorage;
class SceneManager;
class WordPuzzleUI;

class GSUITest final : public GSBaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    std::shared_ptr<Gui::BitmapFont> dialog_font_;
    float test_time_counter_s = 0.0f;

    std::unique_ptr<Gui::Image> test_image_;
    std::unique_ptr<Gui::Image9Patch> test_frame_;
    std::unique_ptr<WordPuzzleUI> word_puzzle_;

    uint32_t sophia_indices_[2] = {0xffffffff};

    void OnPostloadScene(JsObjectP &js_scene) override;

    void UpdateAnim(uint64_t dt_us) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

  public:
    explicit GSUITest(Viewer *viewer);
    ~GSUITest() final;

    void Enter() override;
    void Exit() override;

    bool HandleInput(const Eng::InputManager::Event &evt) override;
};