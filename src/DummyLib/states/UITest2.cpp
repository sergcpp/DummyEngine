#include "UITest2.h"

#include <fstream>
#include <memory>

#include <Eng/Log.h>
#include <Eng/ViewerStateManager.h>
#include <Eng/renderer/Renderer.h>
#include <Eng/scene/SceneManager.h>
#include <Eng/widgets/CmdlineUI.h>
#include <Gui/EditBox.h>
#include <Gui/Image.h>
#include <Gui/Image9Patch.h>
#include <Gui/Utils.h>
#include <Sys/Time_.h>

#include "../Viewer.h"
#include "../utils/Dictionary.h"
#include "../widgets/FontStorage.h"

UITest2::UITest2(Viewer *viewer) : BaseState(viewer) {
    dialog_font_ = viewer->font_storage()->FindFont("book_main_font");
    // dialog_font_->set_scale(1.5f);

    dict_ = viewer->dictionary();

    const float font_height = dialog_font_->height(ui_root_);

    Gui::Image9Patch edit_box_frame{
        *ren_ctx_, "assets_pc/textures/ui/frame_01.dds", Gui::Vec2f{8}, 1, Gui::Vec2f{-1}, Gui::Vec2f{2}, nullptr};
    edit_box_ = std::make_unique<Gui::EditBox>(edit_box_frame, dialog_font_, Gui::Vec2f{-0.5f, 0.75f},
                                               Gui::Vec2f{1.0f, 1.25f * font_height}, ui_root_);
    edit_box_->edit_flags &= ~Gui::Bitmask<Gui::eEditBoxFlags>{Gui::eEditBoxFlags::Multiline};

    edit_box_->updated_signal.Connect<UITest2, &UITest2::UpdateHint>(this);

    results_frame_ =
        std::make_unique<Gui::Image9Patch>(*ren_ctx_, "assets_pc/textures/ui/frame_01.dds", Gui::Vec2f{8.0f}, 1.0f,
                                           Gui::Vec2f{-0.5f, -0.75f}, Gui::Vec2f{1.0f, 1.5f}, ui_root_);
}

UITest2::~UITest2() = default;

void UITest2::Enter() {
    BaseState::Enter();

    log_->Info("GSUITest2: Loading scene!");
    BaseState::LoadScene("scenes/empty.json");

    { // Load dictionary
        std::ifstream dict_file("assets_pc/scenes/test/test_dict/de-en.dict",
                                std::ios::binary);
        dict_->Load(dict_file, log_);
    }
}

void UITest2::OnPostloadScene(Sys::JsObjectP &js_scene) {
    BaseState::OnPostloadScene(js_scene);

    Ren::Vec3d view_origin, view_dir = Ren::Vec3d{0, 0, 1};
    float view_fov = 45, min_exposure = -1000, max_exposure = 1000;

    if (const size_t camera_ndx = js_scene.IndexOf("camera"); camera_ndx < js_scene.Size()) {
        const Sys::JsObjectP &js_cam = js_scene[camera_ndx].second.as_obj();
        if (const size_t view_origin_ndx = js_cam.IndexOf("view_origin"); view_origin_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_orig = js_cam[view_origin_ndx].second.as_arr();
            view_origin[0] = js_orig.at(0).as_num().val;
            view_origin[1] = js_orig.at(1).as_num().val;
            view_origin[2] = js_orig.at(2).as_num().val;
        }
        if (const size_t view_dir_ndx = js_cam.IndexOf("view_dir"); view_dir_ndx < js_cam.Size()) {
            const Sys::JsArrayP &js_dir = js_cam[view_dir_ndx].second.as_arr();
            view_dir[0] = js_dir.at(0).as_num().val;
            view_dir[1] = js_dir.at(1).as_num().val;
            view_dir[2] = js_dir.at(2).as_num().val;
        }
        if (const size_t fov_ndx = js_cam.IndexOf("fov"); fov_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_fov = js_cam[fov_ndx].second.as_num();
            view_fov = float(js_fov.val);
        }
        if (const size_t min_exposure_ndx = js_cam.IndexOf("min_exposure"); min_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_min_exposure = js_cam[min_exposure_ndx].second.as_num();
            min_exposure = float(js_min_exposure.val);
        }
        if (const size_t max_exposure_ndx = js_cam.IndexOf("max_exposure"); max_exposure_ndx < js_cam.Size()) {
            const Sys::JsNumber &js_max_exposure = js_cam.at("max_exposure").as_num();
            max_exposure = float(js_max_exposure.val);
        }
    }

    scene_manager_->SetupView(view_origin, (view_origin + view_dir), Ren::Vec3f{0, 1, 0}, view_fov, Ren::Vec2f{0.0f}, 1,
                              min_exposure, max_exposure);
}

void UITest2::Exit() { BaseState::Exit(); }

void UITest2::DrawUI(Gui::Renderer *r, Gui::BaseElement *root) {
    BaseState::DrawUI(r, root);

    static const uint8_t color_white[] = {255, 255, 255, 255};
    const float font_height = dialog_font_->height(root);

    { // draw results
        float cur_y = 0.75f - font_height;

        for (const std::string &result_line : results_lines_) {
            dialog_font_->DrawText(r, result_line, Gui::Vec2f{-0.49f, cur_y}, color_white, root);
            cur_y -= font_height;
        }
    }
}

void UITest2::UpdateHint(const std::string_view line) {
    results_lines_.clear();

    auto lookup_word = [this](std::string_view word, int mutation_cost) {
        Dictionary::dict_entry_res_t result = {};
        if (dict_->Lookup(word, result)) {
            log_->Info("Result %s", result.trans[0].data());

            auto line = std::string{result.orth};
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
            while (!result.trans[trans_index].empty()) {
                results_lines_.emplace_back("    ");
                results_lines_.back() += result.trans[trans_index];
                trans_index++;
            }
        }
    };

    lookup_word(line, 0);

    MutateWord(line, lookup_word);
}

void UITest2::MutateWord(std::string_view in_word, const std::function<void(const char *, int)> &callback) {
    uint32_t unicode_word[128] = {};
    int unicode_word_len = 0;

    int word_pos = 0;
    while (word_pos < in_word.size()) {
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
