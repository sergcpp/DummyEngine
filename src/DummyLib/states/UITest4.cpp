#include "UITest4.h"

#include <fstream>
#include <memory>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/utils/FreeCamController.h>
#include <Eng/utils/ScriptedDialog.h>
#include <Eng/utils/ScriptedSequence.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/EditBox.h>
#include <Gui/Image.h>
#include <Gui/Image9Patch.h>
#include <Gui/Renderer.h>
#include <Gui/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../utils/DialogController.h"
#include "../widgets/CaptionsUI.h"
#include "../widgets/DialogEditUI.h"
#include "../widgets/DialogUI.h"
#include "../widgets/FontStorage.h"
#include "../widgets/SeqEditUI.h"
#include "../widgets/WordPuzzleUI.h"

namespace UITest4Internal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "courtroom.json";

const char SEQ_NAME[] = "test/test_dialog/0_begin.json";
// const char SEQ_NAME[] = "test/test_seq.json";
} // namespace UITest4Internal

UITest4::UITest4(Viewer *viewer) : BaseState(viewer) {
    dialog_font_ = viewer->font_storage()->FindFont("book_main_font");
    // dialog_font_->set_scale(1.5f);

    const float font_height = dialog_font_->height(ui_root_);

    cam_ctrl_ = std::make_unique<Eng::FreeCamController>(ren_ctx_->w(), ren_ctx_->h(), 0.3f);
    dial_ctrl_ = std::make_unique<DialogController>();

    test_dialog_ = std::make_unique<Eng::ScriptedDialog>(*ren_ctx_, *snd_ctx_, *scene_manager_);

    dialog_ui_ =
        std::make_unique<DialogUI>(Gui::Vec2f{-1, 0}, Gui::Vec2f{2, 1}, ui_root_, *dialog_font_, true /* debug */);
    dialog_ui_->make_choice_signal.Connect<DialogController, &DialogController::MakeChoice>(dial_ctrl_.get());

    seq_edit_ui_ = std::make_unique<SeqEditUI>(*ren_ctx_, *font_, Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_);
    // seq_edit_ui_->set_sequence(/*test_seq_.get()*/ test_dialog_->GetSequence(0));

    dialog_edit_ui_ = std::make_unique<DialogEditUI>(*ren_ctx_, *font_, Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_);
    dialog_edit_ui_->set_dialog(test_dialog_.get());

    dialog_edit_ui_->set_cur_sequence_signal.Connect<DialogController, &DialogController::SetCurSequence>(
        dial_ctrl_.get());

    dialog_edit_ui_->edit_cur_seq_signal.Connect<UITest4, &UITest4::OnEditSequence>(this);

    seq_cap_ui_ = std::make_unique<CaptionsUI>(Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_, *dialog_font_);
    dial_ctrl_->push_caption_signal.Connect<CaptionsUI, &CaptionsUI::OnPushCaption>(seq_cap_ui_.get());
    dial_ctrl_->push_choice_signal.Connect<DialogUI, &DialogUI::OnPushChoice>(dialog_ui_.get());
    dial_ctrl_->switch_sequence_signal.Connect<DialogEditUI, &DialogEditUI::OnSwitchSequence>(dialog_edit_ui_.get());
    dial_ctrl_->start_puzzle_signal.Connect<UITest4, &UITest4::OnStartPuzzle>(this);

    word_puzzle_ = std::make_unique<WordPuzzleUI>(*ren_ctx_, Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_, *dialog_font_);
    word_puzzle_->puzzle_solved_signal.Connect<DialogController, &DialogController::ContinueChoice>(dial_ctrl_.get());
}

UITest4::~UITest4() = default;

void UITest4::Enter() {
    using namespace UITest4Internal;

    BaseState::Enter();

    cmdline_ui_->RegisterCommand("dialog", [this](Ren::Span<const Eng::CmdlineUI::ArgData> args) -> bool {
        LoadDialog(args[1].str.data());
        return true;
    });

    log_->Info("GSUITest: Loading scene!");
    BaseState::LoadScene(SCENE_NAME);

    LoadDialog(SEQ_NAME);
}

void UITest4::LoadDialog(const std::string_view seq_name) {
    auto read_sequence = [](const std::string_view seq_name, Sys::JsObject &js_seq) {
#if defined(__ANDROID__)
        const std::string file_name = std::string("assets/scenes/") + std::string(seq_name);
#else
        const std::string file_name = std::string("assets_pc/scenes/") + std::string(seq_name);
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

    Sys::JsObject js_seq;
    if (!read_sequence(seq_name, js_seq)) {
        log_->Error("Failed to read sequence %s", seq_name.data());
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

bool UITest4::SaveSequence(const std::string_view seq_name) {
    // rotate backup files
    for (int i = 7; i > 0; i--) {
        const std::string name1 = std::string("assets/scenes/") + std::string(seq_name) + std::to_string(i),
                          name2 = std::string("assets/scenes/") + std::string(seq_name) + std::to_string(i + 1);
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

    Sys::JsObject js_seq;
    dial_ctrl_->GetCurSequence()->Save(js_seq);

    const std::string out_file_name = std::string("assets/scenes/") + std::string(seq_name);
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

void UITest4::OnEditSequence(const int id) {
    dial_ctrl_->SetCurSequence(id);
    dial_edit_mode_ = 1;
}

void UITest4::OnStartPuzzle(std::string_view puzzle_name) {
#if defined(__ANDROID__)
    const std::string file_name = std::string("assets/scenes/") + std::string(puzzle_name);
#else
    const std::string file_name = std::string("assets_pc/scenes/") + std::string(puzzle_name);
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

    Sys::JsObject js_puzzle;
    if (!js_puzzle.Read(in_stream)) {
        log_->Error("Failed to parse %s", file_name.c_str());
        return;
    }

    word_puzzle_->Load(js_puzzle);
    word_puzzle_->Restart();
}

void UITest4::OnPostloadScene(Sys::JsObjectP &js_scene) {
    using namespace UITest4Internal;

    BaseState::OnPostloadScene(js_scene);

    if (const size_t camera_ndx = js_scene.IndexOf("camera"); camera_ndx < js_scene.Size()) {
        const Sys::JsObjectP &js_cam = js_scene[camera_ndx].second.as_obj();
        if (const size_t view_origin_ndx = js_cam.IndexOf("view_origin"); view_origin_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_orig = js_cam[view_origin_ndx].second.as_arr();
            cam_ctrl_->view_origin[0] = js_orig.at(0).as_num().val;
            cam_ctrl_->view_origin[1] = js_orig.at(1).as_num().val;
            cam_ctrl_->view_origin[2] = js_orig.at(2).as_num().val;
        }
        if (const size_t view_dir_ndx = js_cam.IndexOf("view_dir"); view_dir_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_dir = js_cam[view_dir_ndx].second.as_arr();
            cam_ctrl_->view_dir[0] = float(js_dir.at(0).as_num().val);
            cam_ctrl_->view_dir[1] = float(js_dir.at(1).as_num().val);
            cam_ctrl_->view_dir[2] = float(js_dir.at(2).as_num().val);
        }
        if (const size_t fwd_speed_ndx = js_cam.IndexOf("fwd_speed"); fwd_speed_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fwd_speed = js_cam[fwd_speed_ndx].second.as_num();
            cam_ctrl_->max_fwd_speed = float(js_fwd_speed.val);
        }
        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            cam_ctrl_->view_fov = float(js_fov.val);
        }
        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam[max_exposure_ndx].second.as_num();
            cam_ctrl_->max_exposure = float(js_max_exposure.val);
        }
    }
}

void UITest4::UpdateAnim(const uint64_t dt_us) {
    using namespace UITest4Internal;

    BaseState::UpdateAnim(dt_us);

    seq_cap_ui_->Clear();
    dialog_ui_->Clear();

    if (dial_ctrl_) {
        dial_ctrl_->Update(Sys::GetTimeS());
    }

    if (use_free_cam_) {
        scene_manager_->SetupView(cam_ctrl_->view_origin, cam_ctrl_->view_origin + Ren::Vec3d(cam_ctrl_->view_dir),
                                  Ren::Vec3f{0, 1, 0}, cam_ctrl_->view_fov, Ren::Vec2f{0.0f}, 1,
                                  cam_ctrl_->min_exposure, cam_ctrl_->max_exposure);
    }
}

void UITest4::Exit() { BaseState::Exit(); }

void UITest4::Draw() {
    using namespace UITest4Internal;

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

    BaseState::Draw();
}

void UITest4::UpdateFixed(const uint64_t dt_us) { cam_ctrl_->Update(dt_us); }

void UITest4::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace UITest4Internal;

    // BaseState::DrawUI(r, root);

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

bool UITest4::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    using namespace Ren;
    using namespace UITest4Internal;

    // pt switch for touch controls
    if (evt.type == Eng::eInputEvent::P1Down || evt.type == Eng::eInputEvent::P2Down) {
        if (evt.point[0] > float(ren_ctx_->w()) * 0.9f && evt.point[1] < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    // scene_manager_->InitScene_PT();
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
    case Eng::eInputEvent::P1Down: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0 && dialog_edit_ui_->Check(p)) {
            // dialog_edit_ui_->Press(p, true);
            input_processed = true;
        } else if (dial_edit_mode_ == 1 && seq_edit_ui_->Check(p)) {
            // seq_edit_ui_->Press(p, true);
            input_processed = true;
        } else if (dialog_ui_->Check(p)) {
            // dialog_ui_->Press(p, true);
        }
        // word_puzzle_->Press(p, true);
    } break;
    case Eng::eInputEvent::P2Down: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
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
    case Eng::eInputEvent::P1Up: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            // dialog_edit_ui_->Press(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            // seq_edit_ui_->Press(p, false);
            input_processed = seq_edit_ui_->Check(p);
        }
        // dialog_ui_->Press(p, false);
        // word_puzzle_->Press(p, false);
        cam_ctrl_->HandleInput(evt);
    } break;
    case Eng::eInputEvent::P2Up: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            dialog_edit_ui_->PressRMB(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            seq_edit_ui_->PressRMB(p, false);
            input_processed = seq_edit_ui_->Check(p);
        }
    } break;
    case Eng::eInputEvent::P1Move: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            // dialog_edit_ui_->Hover(p);
        } else if (dial_edit_mode_ == 1) {
            // seq_edit_ui_->Hover(p);
        }
        // dialog_ui_->Hover(p);
        // word_puzzle_->Hover(p);
    } break;
    case Eng::eInputEvent::P2Move: {

    } break;
    case Eng::eInputEvent::KeyDown: {
        input_processed = false;

        if (evt.key_code == Eng::eKey::LeftShift || evt.key_code == Eng::eKey::RightShift) {
        } else if (evt.key_code == Eng::eKey::Return) {

        } else if (evt.key_code == Eng::eKey::Left) {

        } else if (evt.key_code == Eng::eKey::Right) {

        } else if (evt.key_code == Eng::eKey::Up) {

        } else if (evt.key_code == Eng::eKey::Down) {

        } else if (evt.key_code == Eng::eKey::Delete) {
            if (word_puzzle_->active()) {
                word_puzzle_->Cancel();
                dial_ctrl_->ContinueChoice();
            } else if (dial_edit_mode_ == 1) {
                dial_edit_mode_ = 0;
            }
        } else if (evt.key_code == Eng::eKey::DeleteForward) {
        } else if (evt.key_code == Eng::eKey::Tab) {
            if (dial_edit_mode_ == -1) {
                dial_edit_mode_ = 0;
            } else {
                dial_edit_mode_ = -1;
            }
        } else {
            char ch = Eng::CharFromKeycode(evt.key_code);
            if (keys_state[Eng::eKey::LeftShift] || keys_state[Eng::eKey::RightShift]) {
                if (ch == '-') {
                    ch = '_';
                } else {
                    ch = char(std::toupper(ch));
                }
            }
        }
    } break;
    case Eng::eInputEvent::KeyUp: {
        input_processed = true;
        if (evt.key_code == Eng::eKey::Return) {
            use_free_cam_ = !use_free_cam_;
        } else if (evt.key_code == Eng::eKey::Space) {
            auto dial_state = dial_ctrl_->state();
            if (dial_state == DialogController::eState::Sequence) {
                dial_ctrl_->Pause();
            } else {
                dial_ctrl_->Play(Sys::GetTimeS());
            }
        } else if (evt.key_code == Eng::eKey::F5) {
            Viewer::PrepareAssets("pc");
            trigger_dialog_reload_ = true;
        } else if (evt.key_code == Eng::eKey::F6) {
            const Eng::ScriptedSequence *cur_seq = dial_ctrl_->GetCurSequence();
            SaveSequence(cur_seq->lookup_name());
        } else {
            input_processed = false;
        }
    } break;
    case Eng::eInputEvent::MouseWheel: {
        if (dial_edit_mode_ == 1) {
            if (evt.move[0] > 0) {
                seq_edit_ui_->ZoomInTime();
            } else {
                seq_edit_ui_->ZoomOutTime();
            }
        }
    } break;
    case Eng::eInputEvent::Resize:
        cam_ctrl_->Resize(ren_ctx_->w(), ren_ctx_->h());
        // dialog_ui_->Resize(ui_root_);
        // dialog_edit_ui_->Resize(ui_root_);
        // seq_edit_ui_->Resize(ui_root_);
        // seq_cap_ui_->Resize(ui_root_);
        // word_puzzle_->Resize(ui_root_);
        break;
    default:
        break;
    }

    if (!input_processed) {
        input_processed = cam_ctrl_->HandleInput(evt);
    }

    if (!input_processed) {
        BaseState::HandleInput(evt, keys_state);
    }

    return true;
}
