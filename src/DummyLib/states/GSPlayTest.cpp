#include "GSPlayTest.h"

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
#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../utils/Dictionary.h"
#include "../widgets/CaptionsUI.h"
#include "../widgets/DialogEditUI.h"
#include "../widgets/DialogUI.h"
#include "../widgets/FontStorage.h"
#include "../widgets/SeqEditUI.h"
#include "../widgets/WordPuzzleUI.h"

namespace GSPlayTestInternal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "seq_test.json";

const char SEQ_NAME[] = "test/test_seq.json";
} // namespace GSPlayTestInternal

GSPlayTest::GSPlayTest(Viewer *viewer) : GSBaseState(viewer) {
    dialog_font_ = viewer->font_storage()->FindFont("book_main_font");

    cam_ctrl_ = std::make_unique<Eng::FreeCamController>(ren_ctx_->w(), ren_ctx_->h(), 0.3f);

    test_dialog_ = std::make_unique<Eng::ScriptedDialog>(*ren_ctx_, *snd_ctx_, *scene_manager_);

    dialog_ui_ = std::make_unique<DialogUI>(Gui::Vec2f{-1}, Gui::Vec2f{2}, ui_root_, *dialog_font_);

    seq_edit_ui_ = std::make_unique<SeqEditUI>(*ren_ctx_, *font_, Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_);

    dialog_edit_ui_ = std::make_unique<DialogEditUI>(*ren_ctx_, *font_, Gui::Vec2f{-1}, Gui::Vec2f{2, 1}, ui_root_);
    dialog_edit_ui_->set_dialog(test_dialog_.get());

    dialog_edit_ui_->set_cur_sequence_signal.Connect<GSPlayTest, &GSPlayTest::OnSetCurSequence>(this);

    seq_cap_ui_ = std::make_unique<CaptionsUI>(Gui::Vec2f{-1, 0}, Gui::Vec2f{2, 1}, ui_root_, *dialog_font_);
    // test_seq_->push_caption_signal.Connect<CaptionsUI, &CaptionsUI::OnPushCaption>(
    //    seq_cap_ui_.get());

    /*Gui::Image9Patch edit_box_frame {
            *ctx_, "assets_pc/textures/ui/frame_01.uncompressed.png",
    Ren::Vec2f{ 8, 8 }, 1, Ren::Vec2f{ -1 }, Ren::Vec2f{ 2
    }, ui_root_.get()
    };
    edit_box_ = std::make_unique<Gui::EditBox>(
        edit_box_frame, dialog_font_.get(), Ren::Vec2f{ -0.5f, 0.75f },
        Ren::Vec2f{ 1.0f, 0.75f * font_height },ui_root_.get());
    edit_box_->set_flag(Gui::Multiline, false);

    results_frame_ = std::make_unique<Gui::Image9Patch>(
        *ctx_, "assets_pc/textures/ui/frame_01.uncompressed.png",
    Ren::Vec2f{ 8, 8
    }, 1, Ren::Vec2f{ -0.5f, -0.75f }, Ren::Vec2f{ 1.0f, 1.5f }, ui_root_.get()
        );*/
}

GSPlayTest::~GSPlayTest() = default;

void GSPlayTest::Enter() {
    using namespace GSPlayTestInternal;

    GSBaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    GSBaseState::LoadScene(SCENE_NAME);

    LoadSequence(SEQ_NAME);
}

void GSPlayTest::LoadSequence(const std::string_view seq_name) {
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

    test_seq_ = test_dialog_->GetSequence(0);
    seq_edit_ui_->set_sequence(test_seq_);
}

bool GSPlayTest::SaveSequence(const std::string_view seq_name) {
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
                ren_ctx_->log()->Error("Failed to remove file %s", name2.c_str());
                return false;
            }
        }

        const int ret = std::rename(name1.c_str(), name2.c_str());
        if (ret) {
            ren_ctx_->log()->Error("Failed to rename file %s", name1.c_str());
            return false;
        }
    }

    Sys::JsObject js_seq;
    test_seq_->Save(js_seq);

    const std::string out_file_name = std::string("assets/scenes/") + std::string(seq_name);
    if (std::ifstream(out_file_name).good()) {
        const std::string back_name = out_file_name + "1";
        const int ret = std::rename(out_file_name.c_str(), back_name.c_str());
        if (ret) {
            ren_ctx_->log()->Error("Failed to rename file %s", out_file_name.c_str());
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

void GSPlayTest::OnPostloadScene(Sys::JsObjectP &js_scene) {
    using namespace GSPlayTestInternal;

    GSBaseState::OnPostloadScene(js_scene);

    if (js_scene.Has("camera")) {
        const Sys::JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const Sys::JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            cam_ctrl_->view_origin[0] = float(js_orig.at(0).as_num().val);
            cam_ctrl_->view_origin[1] = float(js_orig.at(1).as_num().val);
            cam_ctrl_->view_origin[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const Sys::JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            cam_ctrl_->view_dir[0] = float(js_dir.at(0).as_num().val);
            cam_ctrl_->view_dir[1] = float(js_dir.at(1).as_num().val);
            cam_ctrl_->view_dir[2] = float(js_dir.at(2).as_num().val);
        }

        if (js_cam.Has("fwd_speed")) {
            const Sys::JsNumber &js_fwd_speed = js_cam.at("fwd_speed").as_num();
            cam_ctrl_->max_fwd_speed = float(js_fwd_speed.val);
        }

        if (js_cam.Has("fov")) {
            const Sys::JsNumber &js_fov = js_cam.at("fov").as_num();
            cam_ctrl_->view_fov = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const Sys::JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            cam_ctrl_->max_exposure = float(js_max_exposure.val);
        }
    }
}

void GSPlayTest::UpdateAnim(const uint64_t dt_us) {
    using namespace GSPlayTestInternal;

    GSBaseState::UpdateAnim(dt_us);

    scene_manager_->SetupView(cam_ctrl_->view_origin, (cam_ctrl_->view_origin + cam_ctrl_->view_dir),
                              Ren::Vec3f{0, 1, 0}, cam_ctrl_->view_fov, Ren::Vec2f{0.0f}, 1, cam_ctrl_->min_exposure,
                              cam_ctrl_->max_exposure);

    const Eng::SceneData &scene = scene_manager_->scene_data();

    seq_cap_ui_->Clear();
    if (test_seq_) {
        test_seq_->Update(double(seq_edit_ui_->GetTime()), true);
    }
}

void GSPlayTest::OnSetCurSequence(const int id) {
    if (test_seq_) {
        test_seq_->push_caption_signal.clear();
    }
    test_seq_ = test_dialog_->GetSequence(id);
    test_seq_->push_caption_signal.Connect<CaptionsUI, &CaptionsUI::OnPushCaption>(seq_cap_ui_.get());
    seq_edit_ui_->set_sequence(test_seq_);
}

void GSPlayTest::Exit() { GSBaseState::Exit(); }

void GSPlayTest::Draw() {
    if (is_playing_) {
        const float cur_time_s = 0.001f * Sys::GetTimeMs();
        if (seq_edit_ui_->timeline_grabbed()) {
            play_started_time_s_ = cur_time_s - seq_edit_ui_->GetTime();
        } else {
            const auto end_time = float(test_seq_->duration());

            float play_time_s = cur_time_s - play_started_time_s_;
            while (play_time_s > end_time) {
                play_started_time_s_ += end_time;
                play_time_s -= end_time;
            }

            seq_edit_ui_->SetTime(play_time_s);
        }
    }

    GSBaseState::Draw();
}

void GSPlayTest::UpdateFixed(const uint64_t dt_us) { cam_ctrl_->Update(dt_us); }

void GSPlayTest::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSPlayTestInternal;

    // GSBaseState::DrawUI(r, root);

    dialog_ui_->Draw(r);
    if (dial_edit_mode_ == 0) {
        seq_edit_ui_->Draw(r);
    } else if (dial_edit_mode_ == 1) {
        dialog_edit_ui_->Draw(r);
    }
    seq_cap_ui_->Draw(r);
}

bool GSPlayTest::HandleInput(const Eng::input_event_t &evt, const std::vector<bool> &keys_state) {
    using namespace Ren;
    using namespace GSPlayTestInternal;

    // pt switch for touch controls
    if (evt.type == Eng::eInputEvent::P1Down || evt.type == Eng::eInputEvent::P2Down) {
        if (evt.point[0] > float(ren_ctx_->w()) * 0.9f && evt.point[1] < float(ren_ctx_->h()) * 0.1f) {
            const uint64_t new_time = Sys::GetTimeMs();
            if (new_time - click_time_ms_ < 400) {
                use_pt_ = !use_pt_;
                if (use_pt_) {
                    // scene_manager_->InitScene_PT();
                    invalidate_view_ = true;
                }
                click_time_ms_ = 0;
            } else {
                click_time_ms_ = new_time;
            }
        }
    }

    bool input_processed = false;

    switch (evt.type) {
    case Eng::eInputEvent::P1Down: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0 && seq_edit_ui_->Check(p)) {
            // seq_edit_ui_->Press(p, true);
            input_processed = true;
        } else if (dial_edit_mode_ == 1 && dialog_edit_ui_->Check(p)) {
            // dialog_edit_ui_->Press(p, true);
            input_processed = true;
        }
    } break;
    case Eng::eInputEvent::P2Down: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0 && seq_edit_ui_->Check(p)) {
            seq_edit_ui_->PressRMB(p, true);
            input_processed = true;
        } else if (dial_edit_mode_ == 1 && dialog_edit_ui_->Check(p)) {
            dialog_edit_ui_->PressRMB(p, true);
            input_processed = true;
        }
    } break;
    case Eng::eInputEvent::P1Up: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            // seq_edit_ui_->Press(p, false);
            input_processed = seq_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            // dialog_edit_ui_->Press(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        }
    } break;
    case Eng::eInputEvent::P2Up: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            seq_edit_ui_->PressRMB(p, false);
            input_processed = seq_edit_ui_->Check(p);
        } else if (dial_edit_mode_ == 1) {
            dialog_edit_ui_->PressRMB(p, false);
            input_processed = dialog_edit_ui_->Check(p);
        }
    } break;
    case Eng::eInputEvent::P1Move: {
        const Gui::Vec2f p = Gui::MapPointToScreen(Gui::Vec2i{int(evt.point[0]), int(evt.point[1])},
                                                   Gui::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        if (dial_edit_mode_ == 0) {
            // seq_edit_ui_->Hover(p);
        } else if (dial_edit_mode_ == 1) {
            // dialog_edit_ui_->Hover(p);
        }
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

        } else if (evt.key_code == Eng::eKey::DeleteForward) {
        } else if (evt.key_code == Eng::eKey::Tab) {
            if (dial_edit_mode_ == 1) {
                dial_edit_mode_ = 0;
            } else {
                dial_edit_mode_ = 1;
            }
        } else {
            char ch = Eng::InputManager::CharFromKeycode(evt.key_code);
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
        if (evt.key_code == Eng::eKey::Space) {
            play_started_time_s_ = 0.001f * Sys::GetTimeMs() - seq_edit_ui_->GetTime();
            is_playing_ = !is_playing_;
        } else if (evt.key_code == Eng::eKey::F5) {
            LoadSequence(SEQ_NAME);
        } else if (evt.key_code == Eng::eKey::F6) {
            SaveSequence(SEQ_NAME);
        } else {
            input_processed = false;
        }
    } break;
    case Eng::eInputEvent::MouseWheel: {
        if (dial_edit_mode_ == 0) {
            if (evt.move[0] > 0) {
                seq_edit_ui_->ZoomInTime();
            } else {
                seq_edit_ui_->ZoomOutTime();
            }
        }
    } break;
    case Eng::eInputEvent::Resize: {
        cam_ctrl_->Resize(ren_ctx_->w(), ren_ctx_->h());
        // dialog_edit_ui_->Resize(ui_root_);
        // seq_edit_ui_->Resize(ui_root_);
    } break;
    default:
        break;
    }

    if (!input_processed) {
        input_processed = cam_ctrl_->HandleInput(evt);
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt, keys_state);
    }

    return true;
}
