#include "GSUITest4.h"

#include <fstream>
#include <memory>

#include <Eng/GameStateManager.h>
#include <Eng/Gui/EditBox.h>
#include <Eng/Gui/Image.h>
#include <Eng/Gui/Image9Patch.h>
#include <Eng/Gui/Renderer.h>
#include <Eng/Gui/Utils.h>
#include <Eng/Renderer/Renderer.h>
#include <Eng/Scene/SceneManager.h>
#include <Eng/Utils/Cmdline.h>
#include <Eng/Utils/FreeCamController.h>
#include <Eng/Utils/ScriptedDialog.h>
#include <Eng/Utils/ScriptedSequence.h>
#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/CaptionsUI.h"
#include "../Gui/DialogEditUI.h"
#include "../Gui/DialogUI.h"
#include "../Gui/FontStorage.h"
#include "../Gui/SeqEditUI.h"
#include "../Gui/WordPuzzleUI.h"
#include "../Utils/DialogController.h"
#include "../Viewer.h"

namespace GSUITest4Internal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "courtroom.json";

const char SEQ_NAME[] = "test/test_dialog/0_begin.json";
// const char SEQ_NAME[] = "test/test_seq.json";
} // namespace GSUITest4Internal

GSUITest4::GSUITest4(GameBase *game) : GSBaseState(game) {
    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    dialog_font_ = fonts->FindFont("book_main_font");
    dialog_font_->set_scale(1.5f);

    const float font_height = dialog_font_->height(ui_root_.get());

    cam_ctrl_.reset(new FreeCamController(ren_ctx_->w(), ren_ctx_->h(), 0.3f));
    dial_ctrl_.reset(new DialogController);

    test_dialog_.reset(new ScriptedDialog{*ren_ctx_, *snd_ctx_, *scene_manager_});

    dialog_ui_.reset(
        new DialogUI{Gui::Vec2f{-1.0f, 0.0f}, Gui::Vec2f{2.0f, 1.0f}, ui_root_.get(), *dialog_font_, true /* debug */});
    dialog_ui_->make_choice_signal.Connect<DialogController, &DialogController::MakeChoice>(dial_ctrl_.get());

    seq_edit_ui_.reset(
        new SeqEditUI{*ren_ctx_, *font_, Gui::Vec2f{-1.0f, -1.0f}, Gui::Vec2f{2.0f, 1.0f}, ui_root_.get()});
    // seq_edit_ui_->set_sequence(/*test_seq_.get()*/ test_dialog_->GetSequence(0));

    dialog_edit_ui_.reset(
        new DialogEditUI{*ren_ctx_, *font_, Gui::Vec2f{-1.0f, -1.0f}, Gui::Vec2f{2.0f, 1.0f}, ui_root_.get()});
    dialog_edit_ui_->set_dialog(test_dialog_.get());

    dialog_edit_ui_->set_cur_sequence_signal.Connect<DialogController, &DialogController::SetCurSequence>(
        dial_ctrl_.get());

    dialog_edit_ui_->edit_cur_seq_signal.Connect<GSUITest4, &GSUITest4::OnEditSequence>(this);

    seq_cap_ui_.reset(new CaptionsUI{Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 1.0f}, ui_root_.get(), *dialog_font_});
    dial_ctrl_->push_caption_signal.Connect<CaptionsUI, &CaptionsUI::OnPushCaption>(seq_cap_ui_.get());
    dial_ctrl_->push_choice_signal.Connect<DialogUI, &DialogUI::OnPushChoice>(dialog_ui_.get());
    dial_ctrl_->switch_sequence_signal.Connect<DialogEditUI, &DialogEditUI::OnSwitchSequence>(dialog_edit_ui_.get());
    dial_ctrl_->start_puzzle_signal.Connect<GSUITest4, &GSUITest4::OnStartPuzzle>(this);

    word_puzzle_.reset(
        new WordPuzzleUI(*ren_ctx_, Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 1.0f}, ui_root_.get(), *dialog_font_));
    word_puzzle_->puzzle_solved_signal.Connect<DialogController, &DialogController::ContinueChoice>(dial_ctrl_.get());
}

GSUITest4::~GSUITest4() = default;

void GSUITest4::Enter() {
    using namespace GSUITest4Internal;

    GSBaseState::Enter();

    std::shared_ptr<GameStateManager> state_manager = state_manager_.lock();
    std::weak_ptr<GSUITest4> weak_this = std::dynamic_pointer_cast<GSUITest4>(state_manager->Peek());

    cmdline_->RegisterCommand("dialog", [weak_this](int argc, Cmdline::ArgData *argv) -> bool {
        auto shrd_this = weak_this.lock();
        if (shrd_this) {
            shrd_this->LoadDialog(argv[1].str.str);
        }
        return true;
    });

    log_->Info("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    LoadDialog(SEQ_NAME);
}

void GSUITest4::LoadDialog(const char *seq_name) {
    auto read_sequence = [](const char *seq_name, JsObject &js_seq) {
#if defined(__ANDROID__)
        const std::string file_name = std::string("assets/scenes/") + seq_name;
#else
        const std::string file_name = std::string("assets_pc/scenes/") + seq_name;
#endif

        Sys::AssetFile in_seq(file_name);
        if (!in_seq) {
            return false;
        }

        const size_t seq_size = in_seq.size();

        std::unique_ptr<uint8_t[]> seq_data(new uint8_t[seq_size]);
        in_seq.Read((char *)&seq_data[0], seq_size);

        Sys::MemBuf mem(&seq_data[0], seq_size);
        std::istream in_stream(&mem);

        return js_seq.Read(in_stream);
    };

    JsObject js_seq;
    if (!read_sequence(seq_name, js_seq)) {
        log_->Error("Failed to read sequence %s", seq_name);
        return;
    }

    test_dialog_->Clear();
    if (!test_dialog_->Load(seq_name, js_seq, read_sequence)) {
        log_->Error("Failed to load dialog");
        return;
    }

    dial_ctrl_->SetDialog(test_dialog_.get());
    dialog_edit_ui_->set_dialog(test_dialog_.get());
    seq_edit_ui_->set_sequence(dial_ctrl_->GetCurSequence());

    use_free_cam_ = false;
}

bool GSUITest4::SaveSequence(const char *seq_name) {
    // rotate backup files
    for (int i = 7; i > 0; i--) {
        const std::string name1 = std::string("assets/scenes/") + seq_name + std::to_string(i),
                          name2 = std::string("assets/scenes/") + seq_name + std::to_string(i + 1);
        if (!std::ifstream(name1).good()) {
            continue;
        }
        if (i == 7 && std::ifstream(name2).good()) {
            const int ret = std::remove(name2.c_str());
            if (ret) {
                log_->Error("Failed to remove file %s", name2.c_str());
                return false;
            }
        }

        const int ret = std::rename(name1.c_str(), name2.c_str());
        if (ret) {
            log_->Error("Failed to rename file %s", name1.c_str());
            return false;
        }
    }

    JsObject js_seq;
    dial_ctrl_->GetCurSequence()->Save(js_seq);

    const std::string out_file_name = std::string("assets/scenes/") + seq_name;
    if (std::ifstream(out_file_name).good()) {
        const std::string back_name = out_file_name + "1";
        const int ret = std::rename(out_file_name.c_str(), back_name.c_str());
        if (ret) {
            log_->Error("Failed to rename file %s", out_file_name.c_str());
            return false;
        }
    }

    { // write out file
        std::ofstream out_file(out_file_name, std::ios::binary);
        if (!out_file) {
            log_->Error("Can not open file %s for writing", out_file_name.c_str());
            return false;
        }

        js_seq.Write(out_file);
    }
    Viewer::PrepareAssets("pc");
    return true;
}

void GSUITest4::OnEditSequence(const int id) {
    dial_ctrl_->SetCurSequence(id);
    dial_edit_mode_ = 1;
}

void GSUITest4::OnStartPuzzle(const char *puzzle_name) {
#if defined(__ANDROID__)
    const std::string file_name = std::string("assets/scenes/") + puzzle_name;
#else
    const std::string file_name = std::string("assets_pc/scenes/") + puzzle_name;
#endif

    Sys::AssetFile in_puzzle(file_name);
    if (!in_puzzle) {
        log_->Error("Failed to load %s", file_name.c_str());
        return;
    }

    const size_t puzzle_size = in_puzzle.size();

    std::unique_ptr<uint8_t[]> puzzle_data(new uint8_t[puzzle_size]);
    in_puzzle.Read((char *)&puzzle_data[0], puzzle_size);

    Sys::MemBuf mem(&puzzle_data[0], puzzle_size);
    std::istream in_stream(&mem);

    JsObject js_puzzle;
    if (!js_puzzle.Read(in_stream)) {
        log_->Error("Failed to parse %s", file_name.c_str());
        return;
    }

    word_puzzle_->Load(js_puzzle);
    word_puzzle_->Restart();
}

void GSUITest4::OnPostloadScene(JsObjectP &js_scene) {
    using namespace GSUITest4Internal;

    GSBaseState::OnPostloadScene(js_scene);

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            cam_ctrl_->view_origin[0] = float(js_orig.at(0).as_num().val);
            cam_ctrl_->view_origin[1] = float(js_orig.at(1).as_num().val);
            cam_ctrl_->view_origin[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            cam_ctrl_->view_dir[0] = float(js_dir.at(0).as_num().val);
            cam_ctrl_->view_dir[1] = float(js_dir.at(1).as_num().val);
            cam_ctrl_->view_dir[2] = float(js_dir.at(2).as_num().val);
        }

        if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            cam_ctrl_->max_fwd_speed = float(js_fwd_speed.val);
        }

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            cam_ctrl_->view_fov = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            cam_ctrl_->max_exposure = float(js_max_exposure.val);
        }
    }
}

void GSUITest4::UpdateAnim(const uint64_t dt_us) {
    using namespace GSUITest4Internal;

    GSBaseState::UpdateAnim(dt_us);

    seq_cap_ui_->Clear();
    dialog_ui_->Clear();

    if (dial_ctrl_) {
        dial_ctrl_->Update(Sys::GetTimeS());
    }

    if (use_free_cam_) {
        scene_manager_->SetupView(cam_ctrl_->view_origin, (cam_ctrl_->view_origin + cam_ctrl_->view_dir),
                                  Ren::Vec3f{0.0f, 1.0f, 0.0f}, cam_ctrl_->view_fov, cam_ctrl_->max_exposure);
    }
}

void GSUITest4::Exit() { GSBaseState::Exit(); }

void GSUITest4::Draw() {
    using namespace GSUITest4Internal;

    if (trigger_dialog_reload_) {
        LoadDialog(SEQ_NAME);
        trigger_dialog_reload_ = false;
    }

    const double cur_time_s = Sys::GetTimeS();
    if (seq_edit_ui_->timeline_grabbed()) {
        const double play_time_s = seq_edit_ui_->GetTime();
        dial_ctrl_->SetPlayTime(cur_time_s, play_time_s);
    } else {
        const double play_time_s = dial_ctrl_->GetPlayTime();
        seq_edit_ui_->SetTime(float(play_time_s));
    }

    GSBaseState::Draw();
}

void GSUITest4::UpdateFixed(const uint64_t dt_us) { cam_ctrl_->Update(dt_us); }

void GSUITest4::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITest4Internal;

    // GSBaseState::DrawUI(r, root);

    dialog_ui_->Draw(r);
    if (dial_edit_mode_ == 0) {
        dialog_edit_ui_->Draw(r);
    } else if (dial_edit_mode_ == 1) {
        seq_edit_ui_->set_sequence(dial_ctrl_->GetCurSequence());
        seq_edit_ui_->Draw(r);
    }
    seq_cap_ui_->Draw(r);
    word_puzzle_->Draw(r);
}

bool GSUITest4::HandleInput(const InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSUITest4Internal;

    // pt switch for touch controls
    if (evt.type == RawInputEv::P1Down || evt.type == RawInputEv::P2Down) {
        if (evt.point.x > float(ren_ctx_->w()) * 0.9f && evt.point.y < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }

                click_time_ = 0;
            } else {
                click_time_ = new_time;
            }
        }
    }

    bool input_processed = false;

    switch (evt.type) {
    case RawInputEv::P1Down: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0 && dialog_edit_ui_->Check(p)) {
            dialog_edit_ui_->Press(p, true);
            input_processed = true;
        } else if (dial_edit_mode_ == 1 && seq_edit_ui_->Check(p)) {
            seq_edit_ui_->Press(p, true);
            input_processed = true;
        } else if (dialog_ui_->Check(p)) {
            dialog_ui_->Press(p, true);
        }
        word_puzzle_->Press(p, true);
    } break;
    case RawInputEv::P2Down: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0 && dialog_edit_ui_->Check(p)) {
            dialog_edit_ui_->PressRMB(p, true);
            input_processed = true;
        } else if (dial_edit_mode_ == 1 && seq_edit_ui_->Check(p)) {
            seq_edit_ui_->PressRMB(p, true);
            input_processed = true;
        } else if (dialog_ui_->Check(p)) {
            // dialog_ui_->PressRMB
        }
    } break;
    case RawInputEv::P1Up: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            dialog_edit_ui_->Press(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            seq_edit_ui_->Press(p, false);
            input_processed = seq_edit_ui_->Check(p);
        }
        dialog_ui_->Press(p, false);
        word_puzzle_->Press(p, false);
        cam_ctrl_->HandleInput(evt);
    } break;
    case RawInputEv::P2Up: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            dialog_edit_ui_->PressRMB(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            seq_edit_ui_->PressRMB(p, false);
            input_processed = seq_edit_ui_->Check(p);
        }
    } break;
    case RawInputEv::P1Move: {
        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            dialog_edit_ui_->Hover(p);
        } else if (dial_edit_mode_ == 1) {
            seq_edit_ui_->Hover(p);
        }
        dialog_ui_->Hover(p);
        word_puzzle_->Hover(p);
    } break;
    case RawInputEv::P2Move: {

    } break;
    case RawInputEv::KeyDown: {
        input_processed = false;

        if (evt.key_code == KeyLeftShift || evt.key_code == KeyRightShift) {
        } else if (evt.key_code == KeyReturn) {

        } else if (evt.key_code == KeyLeft) {

        } else if (evt.key_code == KeyRight) {

        } else if (evt.key_code == KeyUp) {

        } else if (evt.key_code == KeyDown) {

        } else if (evt.key_code == KeyDelete) {
            if (word_puzzle_->active()) {
                word_puzzle_->Cancel();
                dial_ctrl_->ContinueChoice();
            } else if (dial_edit_mode_ == 1) {
                dial_edit_mode_ = 0;
            }
        } else if (evt.key_code == KeyDeleteForward) {
        } else if (evt.key_code == KeyTab) {
            if (dial_edit_mode_ == -1) {
                dial_edit_mode_ = 0;
            } else {
                dial_edit_mode_ = -1;
            }
        } else {
            char ch = InputManager::CharFromKeycode(evt.key_code);
            if (shift_down_) {
                if (ch == '-') {
                    ch = '_';
                } else {
                    ch = char(std::toupper(ch));
                }
            }
        }
    } break;
    case RawInputEv::KeyUp: {
        input_processed = true;
        if (evt.key_code == KeyReturn) {
            use_free_cam_ = !use_free_cam_;
        } else if (evt.key_code == KeySpace) {
            auto dial_state = dial_ctrl_->state();
            if (dial_state == DialogController::eState::Sequence) {
                dial_ctrl_->Pause();
            } else {
                dial_ctrl_->Play(Sys::GetTimeS());
            }
        } else if (evt.key_code == KeyF5) {
            Viewer::PrepareAssets("pc");
            trigger_dialog_reload_ = true;
        } else if (evt.key_code == KeyF6) {
            const ScriptedSequence *cur_seq = dial_ctrl_->GetCurSequence();
            SaveSequence(cur_seq->lookup_name());
        } else {
            input_processed = false;
        }
    } break;
    case RawInputEv::MouseWheel: {
        if (dial_edit_mode_ == 1) {
            if (evt.move.dx > 0.0f) {
                seq_edit_ui_->ZoomInTime();
            } else {
                seq_edit_ui_->ZoomOutTime();
            }
        }
    } break;
    case RawInputEv::Resize:
        cam_ctrl_->Resize(ren_ctx_->w(), ren_ctx_->h());
        dialog_ui_->Resize(ui_root_.get());
        dialog_edit_ui_->Resize(ui_root_.get());
        seq_edit_ui_->Resize(ui_root_.get());
        seq_cap_ui_->Resize(ui_root_.get());
        word_puzzle_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        input_processed = cam_ctrl_->HandleInput(evt);
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}
