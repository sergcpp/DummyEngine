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

class CaptionsUI;
class DialogController;
class DialogEditUI;
class DialogUI;
class Dictionary;
namespace Eng {
class FreeCamController;
class ScriptedDialog;
} // namespace Eng
class ViewerStateManager;
class FontStorage;
class SceneManager;
class ScriptedSequence;
class SeqEditUI;
class WordPuzzleUI;

class UITest4 final : public BaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint64_t click_time_ = 0;

    const Gui::BitmapFont *dialog_font_ = {};
    float test_time_counter_s = 0;

    bool use_free_cam_ = true;
    std::unique_ptr<Eng::FreeCamController> cam_ctrl_;
    std::unique_ptr<DialogController> dial_ctrl_;

    std::unique_ptr<Eng::ScriptedDialog> test_dialog_;
    std::unique_ptr<DialogUI> dialog_ui_;
    std::unique_ptr<WordPuzzleUI> word_puzzle_;

    int dial_edit_mode_ = 0;
    std::unique_ptr<SeqEditUI> seq_edit_ui_;
    std::unique_ptr<DialogEditUI> dialog_edit_ui_;
    std::unique_ptr<CaptionsUI> seq_cap_ui_;

    bool trigger_dialog_reload_ = false;

    void OnPostloadScene(Sys::JsObjectP &js_scene) override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void LoadDialog(std::string_view seq_name);
    bool SaveSequence(std::string_view seq_name);

    void OnEditSequence(int id);
    void OnStartPuzzle(std::string_view puzzle_name);

  public:
    explicit UITest4(Viewer *viewer);
    ~UITest4() final;

    void Enter() override;
    void Exit() override;

    void Draw() override;

    void UpdateFixed(uint64_t dt_us) override;
    void UpdateAnim(uint64_t dt_us) override;

    bool HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) override;
};
