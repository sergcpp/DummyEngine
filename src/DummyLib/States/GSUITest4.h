#pragma once

#include <Eng/GameBase.h>
#include <Eng/GameState.h>
#include <Eng/Gui/BaseElement.h>
#include <Ren/Camera.h>
#include <Ren/MVec.h>
#include <Ren/Mesh.h>
#include <Ren/Program.h>
#include <Ren/SW/SW.h>
#include <Ren/Texture.h>

#include "GSBaseState.h"

class Cmdline;
class CaptionsUI;
class DialogController;
class DialogEditUI;
class DialogUI;
class Dictionary;
class FreeCamController;
class GameStateManager;
class FontStorage;
class SceneManager;
class ScriptedDialog;
class ScriptedSequence;
class SeqEditUI;
class WordPuzzleUI;

class GSUITest4 final : public GSBaseState {
    uint64_t last_frame_time_ = 0;
    double cur_fps_ = 0.0;

    uint32_t click_time_ = 0;

    std::shared_ptr<Gui::BitmapFont> dialog_font_;
    float test_time_counter_s = 0.0f;

    std::unique_ptr<FreeCamController> cam_ctrl_;
    std::unique_ptr<DialogController> dial_ctrl_;

    std::unique_ptr<ScriptedDialog> test_dialog_;
    std::unique_ptr<DialogUI> dialog_ui_;
    std::unique_ptr<WordPuzzleUI> word_puzzle_;

    int dial_edit_mode_ = 0;
    std::unique_ptr<SeqEditUI> seq_edit_ui_;
    std::unique_ptr<DialogEditUI> dialog_edit_ui_;
    std::unique_ptr<CaptionsUI> seq_cap_ui_;

    void OnPostloadScene(JsObject &js_scene) override;

    void OnUpdateScene() override;

    void DrawUI(Gui::Renderer *r, Gui::BaseElement *root) override;

    void LoadSequence(const char *seq_name);
    bool SaveSequence(const char *seq_name);

    void OnEditSequence(int id);
    void OnStartPuzzle(const char *puzzle_name);

  public:
    explicit GSUITest4(GameBase *game);
    ~GSUITest4() final;

    void Enter() override;
    void Exit() override;

    void Draw(uint64_t dt_us) override;

    void Update(uint64_t dt_us) override;

    bool HandleInput(const InputManager::Event &evt) override;
};
