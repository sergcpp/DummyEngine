#include "GSUITest2.h"

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
#include <Ren/Context.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/Json.h>
#include <Sys/MemBuf.h>
#include <Sys/Time_.h>

#include "../Gui/FontStorage.h"
#include "../Utils/Dictionary.h"
#include "../Viewer.h"

namespace GSUITest2Internal {
#if defined(__ANDROID__)
const char SCENE_NAME[] = "assets/scenes/"
#else
const char SCENE_NAME[] = "assets_pc/scenes/"
#endif
                          "zenith.json";
} // namespace GSUITest2Internal

GSUITest2::GSUITest2(Eng::GameBase *game) : GSBaseState(game) {
    const std::shared_ptr<FontStorage> fonts = game->GetComponent<FontStorage>(UI_FONTS_KEY);
    dialog_font_ = fonts->FindFont("book_main_font");
    // dialog_font_->set_scale(1.5f);

    dict_ = game->GetComponent<Dictionary>(DICT_KEY);

    const float font_height = dialog_font_->height(ui_root_.get());

    Gui::Image9Patch edit_box_frame{*ren_ctx_,
                                    "assets_pc/textures/ui/frame_01.uncompressed.png",
                                    Ren::Vec2f{8.0f, 8.0f},
                                    1.0f,
                                    Ren::Vec2f{-1.0f, -1.0f},
                                    Ren::Vec2f{2.0f, 2.0f},
                                    ui_root_.get()};
    edit_box_.reset(new Gui::EditBox{edit_box_frame, dialog_font_.get(), Ren::Vec2f{-0.5f, 0.75f},
                                     Ren::Vec2f{1.0f, 0.75f * font_height}, ui_root_.get()});
    edit_box_->set_flag(Gui::eEditBoxFlags::Multiline, false);

    results_frame_.reset(new Gui::Image9Patch{*ren_ctx_, "assets_pc/textures/ui/frame_01.uncompressed.png",
                                              Ren::Vec2f{8.0f, 8.0f}, 1.0f, Ren::Vec2f{-0.5f, -0.75f},
                                              Ren::Vec2f{1.0f, 1.5f}, ui_root_.get()});
}

GSUITest2::~GSUITest2() = default;

void GSUITest2::Enter() {
    using namespace GSUITest2Internal;

    GSBaseState::Enter();

    log_->Info("GSUITest: Loading scene!");
    // GSBaseState::LoadScene(SCENE_NAME);

    /*{
        std::ifstream dict_file("assets_pc/scenes/test/test_dict/de-en.dict",
                                std::ios::binary);
        dict_->Load(dict_file, log_.get());

        const uint64_t t1_us = Sys::GetTimeUs();

        Dictionary::dict_entry_res_t result = {};
        if (dict_->Lookup("Apfel", result)) {
            volatile int ii = 0;
        }

        const uint64_t t2_us = Sys::GetTimeUs();

        const double t_diff_ms = double(t2_us - t1_us) / 1000.0;
        volatile int ii = 0;
    }*/

    zenith_index_ = scene_manager_->FindObject("zenith");
}

void GSUITest2::OnPostloadScene(JsObjectP &js_scene) {
    using namespace GSUITest2Internal;

    GSBaseState::OnPostloadScene(js_scene);

    Ren::Vec3f view_origin, view_dir = Ren::Vec3f{0.0f, 0.0f, 1.0f};
    float view_fov = 45.0f, max_exposure = 1000.0f;

    if (js_scene.Has("camera")) {
        const JsObjectP &js_cam = js_scene.at("camera").as_obj();
        if (js_cam.Has("view_origin")) {
            const JsArrayP &js_orig = js_cam.at("view_origin").as_arr();
            view_origin[0] = float(js_orig.at(0).as_num().val);
            view_origin[1] = float(js_orig.at(1).as_num().val);
            view_origin[2] = float(js_orig.at(2).as_num().val);
        }

        if (js_cam.Has("view_dir")) {
            const JsArrayP &js_dir = js_cam.at("view_dir").as_arr();
            view_dir[0] = float(js_dir.at(0).as_num().val);
            view_dir[1] = float(js_dir.at(1).as_num().val);
            view_dir[2] = float(js_dir.at(2).as_num().val);
        }

        /*if (js_cam.Has("fwd_speed")) {
            const JsNumber &js_fwd_speed = (const JsNumber &)js_cam.at("fwd_speed");
            max_fwd_speed_ = float(js_fwd_speed.val);
        }*/

        if (js_cam.Has("fov")) {
            const JsNumber &js_fov = js_cam.at("fov").as_num();
            view_fov = float(js_fov.val);
        }

        if (js_cam.Has("max_exposure")) {
            const JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure = float(js_max_exposure.val);
        }
    }

    scene_manager_->SetupView(view_origin, (view_origin + view_dir), Ren::Vec3f{0.0f, 1.0f, 0.0f}, view_fov,
                              true, max_exposure);
}

void GSUITest2::UpdateAnim(const uint64_t dt_us) {
    using namespace GSUITest2Internal;

    GSBaseState::UpdateAnim(dt_us);

    const float delta_time_s = fr_info_.delta_time_us * 0.000001f;
    /*test_time_counter_s += delta_time_s;

    const float char_period_s = 0.025f;

    while (test_time_counter_s > char_period_s) {
        text_printer_->incr_progress();
        test_time_counter_s -= char_period_s;
    }*/

    const Eng::SceneData &scene = scene_manager_->scene_data();

    if (zenith_index_ != 0xffffffff) {
        Eng::SceneObject *zenith = scene_manager_->GetObject(zenith_index_);

        uint32_t mask = Eng::CompDrawableBit | Eng::CompAnimStateBit;
        if ((zenith->comp_mask & mask) == mask) {
            auto *dr = (Eng::Drawable *)scene.comp_store[Eng::CompDrawable]->Get(zenith->components[Eng::CompDrawable]);
            auto *as =
                (Eng::AnimState *)scene.comp_store[Eng::CompAnimState]->Get(zenith->components[Eng::CompAnimState]);

            // keep previous palette for velocity calculation
            std::swap(as->matr_palette_curr, as->matr_palette_prev);
            as->anim_time_s += delta_time_s;

            Ren::Mesh *mesh = dr->mesh.get();
            Ren::Skeleton *skel = mesh->skel();

            const int anim_index = 0;

            skel->UpdateAnim(anim_index, as->anim_time_s);
            skel->ApplyAnim(anim_index);
            skel->UpdateBones(&as->matr_palette_curr[0]);
        }
    }
}

void GSUITest2::Exit() { GSBaseState::Exit(); }

void GSUITest2::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    using namespace GSUITest2Internal;

    // GSBaseState::DrawUI(r, root);

    edit_box_->Draw(r);
    results_frame_->Draw(r);

    static const uint8_t color_white[] = {255, 255, 255, 255};
    const float font_height = dialog_font_->height(root);

    { // draw results
        float cur_y = 0.75f - font_height;

        for (const std::string &result_line : results_lines_) {
            dialog_font_->DrawText(r, result_line.c_str(), Ren::Vec2f{-0.49f, cur_y}, color_white, root);
            cur_y -= font_height;
        }
    }
}

void GSUITest2::UpdateHint() {
    const std::string &line = edit_box_->line_text(0);

    results_lines_.clear();

    auto lookup_word = [this](const char *word, int mutation_cost) {
        Dictionary::dict_entry_res_t result = {};
        if (dict_->Lookup(word, result)) {
            log_->Info("Result %s", result.trans[0]);

            std::string line = result.orth;
            if (result.pos == eGramGrpPos::Noun) {
                line += " (noun";

                if (result.num == eGramGrpNum::Singular) {
                    line += ", singular";

                    if (result.gen == eGramGrpGen::Masculine) {
                        line += ", masculine)";
                    } else if (result.gen == eGramGrpGen::Feminine) {
                        line += ", feminine)";
                    } else {
                        line += ", neutral)";
                    }
                } else {
                    line += ", plural)";
                }
            } else if (result.pos == eGramGrpPos::Verb) {
                line += " (verb)";
            } else if (result.pos == eGramGrpPos::Adjective) {
                line += " (adjective)";
            }

            results_lines_.emplace_back(std::move(line));

            int trans_index = 0;
            while (result.trans[trans_index]) {
                results_lines_.emplace_back("    ");
                results_lines_.back() += result.trans[trans_index];
                trans_index++;
            }
        }
    };

    lookup_word(line.c_str(), 0);

    MutateWord(line.c_str(), lookup_word);
}

void GSUITest2::MutateWord(const char *in_word, const std::function<void(const char *, int)> &callback) {
    uint32_t unicode_word[128] = {};
    int unicode_word_len = 0;

    int word_pos = 0;
    while (in_word[word_pos]) {
        uint32_t unicode;
        word_pos += Gui::ConvChar_UTF8_to_Unicode(&in_word[word_pos], unicode);
        unicode_word[unicode_word_len++] = unicode;
    }

    struct mutation_ctx_t {
        std::function<void(mutation_ctx_t &ctx, uint32_t *, int)> mutation_chain[8];
        int mutation_index = 0;
    } ctx;

    auto split_word_in_two = [](mutation_ctx_t &ctx, uint32_t *unicode_word, int mutation_cost) {
        const auto &next_mutation = ctx.mutation_chain[++ctx.mutation_index];

        int i = 0;
        while (unicode_word[i]) {
            // first part
            if (i != 0) {
                uint32_t temp_char = unicode_word[i];
                unicode_word[i] = 0;

                next_mutation(ctx, unicode_word, mutation_cost + 1);
                unicode_word[i] = temp_char;
            }

            // second part
            next_mutation(ctx, &unicode_word[i], mutation_cost + 1);

            i++;
        }

        ctx.mutation_index--;
    };

    auto swap_character_pairs = [](mutation_ctx_t &ctx, uint32_t *unicode_word, int mutation_cost) {
        const auto &next_mutation = ctx.mutation_chain[++ctx.mutation_index];

        int i = 0;
        while (unicode_word[i]) {
            // swap chars
            std::swap(unicode_word[i], unicode_word[i + 1]);

            next_mutation(ctx, unicode_word, mutation_cost + 1);

            // revert back
            std::swap(unicode_word[i], unicode_word[i + 1]);

            i++;
        }

        ctx.mutation_index--;
    };

    auto output_utf8 = [&callback](mutation_ctx_t &ctx, uint32_t *unicode_word, int mutation_cost) {
        char utf8_word[512];
        int utf8_word_len = 0;

        int j = 0;
        while (unicode_word[j]) {
            utf8_word_len += Gui::ConvChar_Unicode_to_UTF8(unicode_word[j], &utf8_word[utf8_word_len]);
            j++;
        }
        utf8_word[utf8_word_len] = '\0';

        callback(utf8_word, mutation_cost);
    };

    ctx.mutation_index = 0;
    ctx.mutation_chain[0] = split_word_in_two;
    ctx.mutation_chain[1] = swap_character_pairs;
    ctx.mutation_chain[2] = output_utf8;

    ctx.mutation_chain[0](ctx, unicode_word, 0);
}

bool GSUITest2::HandleInput(const Eng::InputManager::Event &evt) {
    using namespace Ren;
    using namespace GSUITest2Internal;

    // pt switch for touch controls
    if (evt.type == Eng::RawInputEv::P1Down || evt.type == Eng::RawInputEv::P2Down) {
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

    bool input_processed = true;

    switch (evt.type) {
    case Eng::RawInputEv::P1Down: {
        Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                             Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        // text_printer_->Press(p, true);
        edit_box_->Press(p, true);
    } break;
    case Eng::RawInputEv::P2Down: {

    } break;
    case Eng::RawInputEv::P1Up: {
        // text_printer_->skip();

        const Ren::Vec2f p = Gui::MapPointToScreen(Ren::Vec2i{int(evt.point.x), int(evt.point.y)},
                                                   Ren::Vec2i{ren_ctx_->w(), ren_ctx_->h()});
        // text_printer_->Press(p, false);
        edit_box_->Press(p, false);

        is_visible_ = !is_visible_;
    } break;
    case Eng::RawInputEv::P2Up:
    case Eng::RawInputEv::P2Move: {

    } break;
    case Eng::RawInputEv::KeyDown: {
        input_processed = false;

        if (evt.key_code == Eng::KeyLeftShift || evt.key_code == Eng::KeyRightShift) {
        } else if (evt.key_code == Eng::KeyReturn) {
            edit_box_->InsertLine({});
        } else if (evt.key_code == Eng::KeyLeft) {
            edit_box_->MoveCursorH(-1);
        } else if (evt.key_code == Eng::KeyRight) {
            edit_box_->MoveCursorH(1);
        } else if (evt.key_code == Eng::KeyUp) {
            edit_box_->MoveCursorV(-1);
        } else if (evt.key_code == Eng::KeyDown) {
            edit_box_->MoveCursorV(1);
        } else if (evt.key_code == Eng::KeyDelete) {
            edit_box_->DeleteBck();
        } else if (evt.key_code == Eng::KeyDeleteForward) {
            edit_box_->DeleteFwd();
        } else {
            char ch = Eng::InputManager::CharFromKeycode(evt.key_code);
            if (shift_down_) {
                if (ch == '-') {
                    ch = '_';
                } else {
                    ch = char(std::toupper(ch));
                }
            }

            edit_box_->AddChar(ch);
        }

        UpdateHint();
    } break;
    case Eng::RawInputEv::KeyUp: {
        if (evt.key_code == Eng::KeyUp || (evt.key_code == Eng::KeyW && !cmdline_enabled_)) {
            // text_printer_->restart();
        } else {
            input_processed = false;
        }
    } break;
    case Eng::RawInputEv::Resize:
        edit_box_->Resize(ui_root_.get());
        break;
    default:
        break;
    }

    if (!input_processed) {
        GSBaseState::HandleInput(evt);
    }

    return true;
}
