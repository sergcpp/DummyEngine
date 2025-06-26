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

#include "BaseState.h"

class DebugInfoUI;
class Dictionary;
class ViewerStateManager;
class FontStorage;
class SceneManager;
class WordPuzzleUI;

class UITest2 : public BaseState {
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

    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void UpdateHint(std::string_view line);
    static void MutateWord(std::string_view in_word, const std::function<void(const char *, int)> &callback);

  public:
    explicit UITest2(Viewer *viewer);
    ~UITest2() final;

    void Enter() override;
    void Exit() override;
};