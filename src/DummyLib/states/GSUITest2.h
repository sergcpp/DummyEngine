#pragma once

#include <Eng/ViewerBase.h>
#include <Eng/ViewerState.h>
#include <Gui/BaseElement.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

#include "GSBaseState.h"

class DebugInfoUI;
class Dictionary;
class ViewerStateManager;
class FontStorage;
class SceneManager;
class WordPuzzleUI;

class GSUITest2 : public GSBaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    const Gui::BitmapFont *dialog_font_ = {};
    float test_time_counter_s = 0;
    bool is_visible_ = false;

    Dictionary *dict_ = nullptr;

    std::unique_ptr<Gui::EditBox> edit_box_;
    std::unique_ptr<Gui::Image9Patch> results_frame_;

    std::vector<std::string> results_lines_;

    uint32_t zenith_index_ = 0xffffffff;

    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void UpdateHint();
    static void MutateWord(std::string_view in_word, const std::function<void(const char *, int)> &callback);

  public:
    explicit GSUITest2(Viewer *viewer);
    ~GSUITest2() final;

    void Enter() override;
    void Exit() override;

    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};