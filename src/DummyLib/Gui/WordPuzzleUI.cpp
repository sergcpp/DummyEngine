#include "WordPuzzleUI.h"

#include <random>

#include <Eng/gui/BitmapFont.h>
#include <Eng/gui/Utils.h>
#include <Ren/Context.h>
#include <Sys/Json.h>
#include <Sys/Time_.h>

namespace WordPuzzleUIInternal {
const char Frame01[] =
#if defined(__ANDROID__)
    "assets/"
#else
    "assets_pc/"
#endif
    "textures/ui/frame_01.uncompressed.png";
const char Frame02[] =
#if defined(__ANDROID__)
    "assets/"
#else
    "assets_pc/"
#endif
    "textures/ui/frame_02.uncompressed.png";

const float SideMarginPx = 16.0f;
const float TopMarginPx = 24.0f;
const float BottomMarginPx = 24.0f;
} // namespace WordPuzzleUIInternal

WordPuzzleUI::WordPuzzleUI(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                           const Gui::BitmapFont &font)
    : Gui::BaseElement(pos, size, parent), font_(font),
      background_small_(ctx, WordPuzzleUIInternal::Frame01, Ren::Vec2f{3.0f, 3.0f}, 1.0f, Ren::Vec2f{0.0f, 0.0f},
                        Ren::Vec2f{1.0f, 1.0f}, this),
      background_large_(ctx, WordPuzzleUIInternal::Frame02, Ren::Vec2f{20.0f, 20.0f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f},
                        Ren::Vec2f{2.0f, 2.0f}, this) {
    log_ = ctx.log();
}

void WordPuzzleUI::Cancel() { state_ = eState::Solved; }

void WordPuzzleUI::Restart() {
    state_ = eState::AnimIntro;
    anim_started_time_s_ = Sys::GetTimeS();
}

bool WordPuzzleUI::Load(const JsObject &js_puzzle) {
    if (!js_puzzle.Has("text")) {
        return false;
    }

    text_data_.clear();
    text_splits_.clear();
    text_options_.clear();
    text_hints_.clear();
    option_variants_.clear();
    hint_strings_.clear();

    std::mt19937 rand_gen(std::random_device{}());

    const JsString &js_text = js_puzzle.at("text").as_str();
    text_data_ = js_text.val;

    if (js_puzzle.Has("options")) {
        const JsArray &js_options = js_puzzle.at("options").as_arr();
        for (const JsElement &js_opt_el : js_options.elements) {
            const JsObject &js_opt = js_opt_el.as_obj();

            OptionData &opt_data = text_options_.emplace_back();
            opt_data.var_start = (int)option_variants_.size();
            opt_data.var_count = 0;
            opt_data.var_correct = opt_data.var_start;
            opt_data.var_selected = -1;
            opt_data.is_hover = false;

            const JsArray &js_variants = js_opt.at("variants").as_arr();
            for (const JsElement &js_var_el : js_variants.elements) {
                const JsString &js_var = js_var_el.as_str();

                option_variants_.push_back(js_var.val);
                opt_data.var_count++;
            }

            const std::string right_answer = option_variants_[opt_data.var_correct];
            std::shuffle(option_variants_.begin() + opt_data.var_start,
                         option_variants_.begin() + opt_data.var_start + opt_data.var_count, rand_gen);

            auto it = std::find(option_variants_.begin() + opt_data.var_start,
                                option_variants_.begin() + opt_data.var_start + opt_data.var_count, right_answer);
            assert(it != option_variants_.begin() + opt_data.var_start + opt_data.var_count);

            opt_data.var_correct = (int)std::distance(option_variants_.begin(), it);
        }
    }

    if (js_puzzle.Has("hints")) {
        const JsArray &js_hints = js_puzzle.at("hints").as_arr();
        for (const JsElement &js_hint_el : js_hints.elements) {
            const JsObject &js_hint = js_hint_el.as_obj();

            HintData &hint_data = text_hints_.emplace_back();
            hint_data.str_index = (int)hint_strings_.size();
            hint_data.is_hover = false;

            const JsString &js_hint_en = js_hint.at("en").as_str();
            hint_strings_.push_back(js_hint_en.val);
        }
    }

    { // parse options
        int word_index = 0, option_index = 0, hint_index = 0;

        int char_pos = 0;
        while (text_data_[char_pos]) {
            uint32_t unicode;
            char_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data_[char_pos], unicode);

            // parse tag
            if (unicode == Gui::g_unicode_less_than) {
                char tag_str[32];
                int tag_str_len = 0;

                while (unicode != Gui::g_unicode_greater_than) {
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data_[char_pos], unicode);
                    tag_str[tag_str_len++] = (char)unicode;
                }
                tag_str[tag_str_len - 1] = '\0';

                if (strcmp(tag_str, "option") == 0) {
                    text_options_[option_index].pos = char_pos;
                } else if (strcmp(tag_str, "/option") == 0) {
                    text_options_[option_index].len = char_pos - 9 - text_options_[option_index].pos;
                    option_index++;
                } else if (strcmp(tag_str, "hint") == 0) {
                    text_hints_[hint_index].pos = char_pos;
                } else if (strcmp(tag_str, "/hint") == 0) {
                    text_hints_[hint_index].len = char_pos - 7 - text_hints_[hint_index].pos;
                    hint_index++;
                } else if (strcmp(tag_str, "split") == 0) {
                    SplitData &sd = text_splits_.emplace_back();
                    sd.pos = char_pos;
                    sd.hint_start = hint_index;
                    sd.option_start = option_index;
                } else if (strcmp(tag_str, "/split") == 0) {
                    SplitData &sd = text_splits_.back();
                    sd.len = char_pos - 8 - sd.pos;
                }

                continue;
            }
        }
    }

    avail_splits_.clear();
    chosen_splits_.clear();

    for (int i = 0; i < (int)text_splits_.size(); i++) {
        avail_splits_.push_back(i);
    }

    std::shuffle(begin(avail_splits_), end(avail_splits_), rand_gen);

    for (int i = 0; i < int(text_splits_.size()); i++) {
        SplitData &sd = text_splits_[avail_splits_[i]];
        sd.slot_index = i;
    }

    return true;
}

void WordPuzzleUI::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);

    background_large_.Resize(this);
}

void WordPuzzleUI::Draw(Gui::Renderer *r) {
    using namespace WordPuzzleUIInternal;
    if (state_ == eState::Solved) {
        return;
    }

    const double cur_time_s = Sys::GetTimeS();
    UpdateState(cur_time_s);

    const double anim_param = std::min(cur_time_s - anim_started_time_s_, 1.0);
    float anim_y_off = -2.0f + 2.0f * float(anim_param);
    if (state_ >= eState::AnimOutro) {
        anim_y_off = -4.0f * float(std::max(anim_param - 0.5, 0.0));
    }

    // draw backdrop
    background_large_.Resize(Ren::Vec2f{-1.0f, -1.0f + anim_y_off}, Ren::Vec2f{2.0f, 2.0f}, this);
    background_large_.Draw(r);

    const float side_margin = SideMarginPx / dims_px_[1][0];
    const float top_margin = TopMarginPx / dims_px_[1][1];
    const float bottom_margin = BottomMarginPx / dims_px_[1][1];
    const float font_height = font_.height(this);

    options_rects_.clear();
    hint_rects_.clear();
    expanded_rects_.clear();

    // draw split rects from previous frame
    for (const rect_t &rect : split_rects_) {
        background_small_.Resize(rect.dims[0], rect.dims[1], this);
        background_small_.Draw(r);
    }

    split_rects_.clear();

    if (state_ < eState::Correcting) {
        { // draw chosen splits
            auto draw_offset = Ren::Vec2f{-1.0f + side_margin, 1.0f - top_margin - font_height + anim_y_off};

            for (const int i : chosen_splits_) {
                const SplitData &sd = text_splits_[i];

                rect_t &rect = split_rects_.emplace_back();
                rect.dims[0] = Ren::Vec2f{draw_offset[0], draw_offset[1] - 0.125f * font_height};

                draw_offset = DrawTextBuffer(r, &text_data_[sd.pos], sd.len, draw_offset, options_rects_,
                                             sd.option_start, hint_rects_, sd.hint_start);

                rect.dims[1] = Ren::Vec2f{draw_offset[0] - rect.dims[0][0], 1.25f * font_height};
            }
        }

        { // draw available splits
            const auto draw_offset_left = Ren::Vec2f{-1.0f + side_margin, -1.0f + bottom_margin + anim_y_off},
                       draw_offset_middle = Ren::Vec2f{-0.333f + side_margin, -1.0f + bottom_margin + anim_y_off},
                       draw_offset_right = Ren::Vec2f{0.333f, -1.0f + bottom_margin + anim_y_off};

            assert(avail_splits_.size() <= 12);
            Ren::Vec2f draw_offsets[12];
            for (int i = 0; i < 12; i++) {
                if (i % 3 == 0) {
                    draw_offsets[i] = draw_offset_left + Ren::Vec2f{0.0f, float(i / 3) * 1.25f * font_height};
                } else if (i % 3 == 1) {
                    draw_offsets[i] = draw_offset_middle + Ren::Vec2f{0.0f, float(i / 3) * 1.25f * font_height};
                } else {
                    draw_offsets[i] = draw_offset_right + Ren::Vec2f{0.0f, float(i / 3) * 1.25f * font_height};
                }
            }

            for (const int i : avail_splits_) {
                const SplitData &sd = text_splits_[i];
                const Ren::Vec2f &draw_offset = draw_offsets[sd.slot_index];

                rect_t &rect = split_rects_.emplace_back();
                rect.dims[0] = Ren::Vec2f{draw_offset[0], draw_offset[1] - 0.125f * font_height};

                int expanded_option = -1;
                const Ren::Vec2f new_draw_offset =
                    DrawTextBuffer(r, &text_data_[sd.pos], sd.len, draw_offset, options_rects_, sd.option_start,
                                   hint_rects_, sd.hint_start);

                rect.dims[1] = Ren::Vec2f{new_draw_offset[0] - rect.dims[0][0], 1.25f * font_height};
            }
        }
    } else {
        UpdateTextBuffer();

        auto draw_offset = Ren::Vec2f{-1.0f + side_margin, 1.0f - top_margin - font_height + anim_y_off};

        DrawTextBuffer(r, preprocessed_text_data_.c_str(), (int)preprocessed_text_data_.size(), draw_offset,
                       options_rects_, 0, hint_rects_, 0);

        // ignore splits when sentence is already constructed
        split_rects_.clear();
    }

    // draw option selector
    if (expanded_option_ != -1) {
        const rect_t &opt_rect = options_rects_[expanded_option_];
        const OptionData &opt = text_options_[opt_rect.data];

        Ren::Vec2f opt_pos = opt_rect.dims[0] + Ren::Vec2f{0.0f, font_height};

        auto exp_back_pos = Gui::Vec2f{opt_pos[0] - 0.1f * font_height, opt_pos[1] - 0.25f * font_height},
             exp_back_size = Gui::Vec2f{0.0f, 0.25f * font_height};

        for (int i = 0; i < opt.var_count; i++) {
            const std::string &var = option_variants_[opt.var_start + i];

            const float width = font_.GetWidth(var.c_str(), -1, this);
            exp_back_size[0] = std::max(exp_back_size[0], width);
            exp_back_size[1] += font_height;
        }

        exp_back_size[0] += 0.2f * font_height;

        background_small_.Resize(exp_back_pos, exp_back_size, this);
        background_small_.Draw(r);

        for (int i = 0; i < opt.var_count; i++) {
            const std::string &var = option_variants_[opt.var_start + i];

            const float width =
                font_.DrawText(r, var.c_str(), opt_pos, (i == hover_var_) ? Gui::ColorRed : Gui::ColorWhite, this);

            rect_t &rect = expanded_rects_.emplace_back();
            rect.dims[0] = opt_pos;
            rect.dims[1] = Gui::Vec2f{width, font_height * 0.8f};

            opt_pos[1] += font_height;
        }
    }

    if (expanded_hint_ != -1) {
        const rect_t &hint_rect = hint_rects_[expanded_hint_];
        const HintData &hint_data = text_hints_[hint_rect.data];

        const float width = font_.GetWidth(hint_strings_[hint_data.str_index].c_str(), -1, this);
        const Ren::Vec2f hint_pos = hint_rect.dims[0] + Ren::Vec2f{0.0f, font_height};

        background_small_.Resize(Gui::Vec2f{hint_pos[0] - 0.1f * font_height, hint_pos[1] - 0.25f * font_height},
                                 Gui::Vec2f{width + 0.2f * font_height, 1.25 * font_height}, this);
        background_small_.Draw(r);

        font_.DrawText(r, hint_strings_[hint_data.str_index].c_str(), hint_pos, Gui::ColorWhite, this);
    }
}

void WordPuzzleUI::Hover(const Gui::Vec2f &p) {
    BaseElement::Hover(p);

    for (OptionData &opt : text_options_) {
        opt.is_hover = false;
    }

    for (HintData &hint : text_hints_) {
        hint.is_hover = false;
    }

    const Ren::Vec2f lp = ToLocal(p);

    hover_var_ = -1;
    if (expanded_option_ != -1) {
        for (int i = 0; i < (int)expanded_rects_.size(); i++) {
            const rect_t &rect = expanded_rects_[i];

            if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
                lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {
                hover_var_ = i;
            }
        }

        return;
    }

    for (int i = 0; i < (int)options_rects_.size() && state_ == eState::Correcting; i++) {
        const rect_t &rect = options_rects_[i];
        OptionData &opt = text_options_[rect.data];

        if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {
            opt.is_hover = true;
        }
    }

    expanded_hint_ = -1;

    for (int i = 0; i < (int)hint_rects_.size() && state_ > eState::AnimIntro; i++) {
        const rect_t &rect = hint_rects_[i];
        HintData &opt = text_hints_[rect.data];

        if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {
            expanded_hint_ = i;
            opt.is_hover = true;
        }
    }
}

void WordPuzzleUI::Press(const Gui::Vec2f &p, const bool push) {
    BaseElement::Press(p, push);

    const Ren::Vec2f lp = ToLocal(p);

    if (expanded_option_ != -1) {
        OptionData &opt = text_options_[expanded_option_];

        for (int i = 0; i < int(expanded_rects_.size()); i++) {
            const rect_t &rect = expanded_rects_[i];

            if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
                lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {

                if (push) {
                    return;
                } else {
                    opt.var_selected = i;
                }
            }
        }
    }

    expanded_option_ = -1;

    for (int i = 0; i < int(options_rects_.size()) && state_ == eState::Correcting; i++) {
        const rect_t &rect = options_rects_[i];
        OptionData &opt = text_options_[rect.data];

        if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {

            if (push) {
                opt.is_pressed = true;
            } else {
                /*if (i == expanded_option_) {
                    opt.is_expanded = false;
                } else*/
                if (opt.is_pressed) {
                    expanded_option_ = i;
                }
            }
        } else {
            opt.is_pressed = false;
        }
    }

    for (int i = 0; i < int(split_rects_.size()) && push && state_ == eState::Building; i++) {
        const rect_t &rect = split_rects_[i];

        if (lp[0] >= rect.dims[0][0] && lp[1] >= rect.dims[0][1] && lp[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            lp[1] <= rect.dims[0][1] + rect.dims[1][1]) {

            if (i < int(chosen_splits_.size())) {
                const int split_index = chosen_splits_[i];
                chosen_splits_.erase(chosen_splits_.begin() + i);

                avail_splits_.push_back(split_index);
            } else {
                const int index = i - int(chosen_splits_.size());
                const int split_index = avail_splits_[index];
                avail_splits_.erase(avail_splits_.begin() + index);

                chosen_splits_.push_back(split_index);
            }

            for (HintData &hint : text_hints_) {
                hint.is_hover = false;
            }
            expanded_hint_ = -1;

            break;
        }
    }
}

void WordPuzzleUI::UpdateTextBuffer() {
    // replace options
    preprocessed_text_data_ = text_data_;
    for (auto it = text_options_.rbegin(); it != text_options_.rend(); ++it) {
        if (it->var_selected != -1) {
            const std::string &sel_var = option_variants_[it->var_start + it->var_selected];

            preprocessed_text_data_.erase(it->pos, it->len);
            preprocessed_text_data_.insert(it->pos, sel_var);
        }
    }
}

void WordPuzzleUI::UpdateState(const double cur_time_s) {
    if (state_ == eState::AnimIntro) {
        if (cur_time_s - anim_started_time_s_ > 1.0) {
            state_ = eState::Building;
        }
    } else if (state_ == eState::Building) {
        if (avail_splits_.empty()) {
            bool is_order_correct = true;

            for (int i = 0; i < (int)chosen_splits_.size(); i++) {
                if (i != chosen_splits_[i]) {
                    is_order_correct = false;
                    break;
                }
            }

            if (is_order_correct) {
                state_ = eState::Correcting;
            }
        }
    } else if (state_ == eState::Correcting) {
        bool all_options_correct = true;

        for (const OptionData &opt : text_options_) {
            if (opt.var_start + opt.var_selected != opt.var_correct) {
                all_options_correct = false;
                break;
            }
        }

        if (all_options_correct) {
            anim_started_time_s_ = cur_time_s;
            state_ = eState::AnimOutro;
        }
    } else if (state_ == eState::AnimOutro) {
        if (cur_time_s - anim_started_time_s_ > 1.0) {
            puzzle_solved_signal.FireN();
            state_ = eState::Solved;
        }
    }
}

Ren::Vec2f WordPuzzleUI::DrawTextBuffer(Gui::Renderer *r, const char *text_data, const int len, Ren::Vec2f draw_offset,
                                        std::vector<rect_t> &out_options_rects, const int option_start,
                                        std::vector<rect_t> &out_hint_rects, const int hint_start) {
    using namespace WordPuzzleUIInternal;

    char portion_buf[4096];
    int portion_buf_len = 0;

    const float side_margin = SideMarginPx / dims_px_[1][0];
    const float font_height = font_.height(this);

    uint8_t text_color_stack[32][4] = {{255, 255, 255, 255}};
    int text_color_stack_size = 1;

    int option_count = 0, hint_count = 0;

    int char_pos = 0;
    while (char_pos < len) {
        if (!text_data[char_pos]) {
            break;
        }
        int char_start = char_pos;

        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data[char_pos], unicode);

        const uint8_t *cur_color = text_color_stack[text_color_stack_size - 1];

        bool draw = false, new_line = false, pop_color = false;

        // parse tag
        if (unicode == Gui::g_unicode_less_than) {
            char tag_str[32];
            int tag_str_len = 0;

            while (unicode != Gui::g_unicode_greater_than) {
                char_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data[char_pos], unicode);
                tag_str[tag_str_len++] = (char)unicode;
            }
            tag_str[tag_str_len - 1] = '\0';

            const uint8_t *push_color = nullptr;

            if (strcmp(tag_str, "red") == 0) {
                push_color = Gui::ColorRed;
            } else if (strcmp(tag_str, "cyan") == 0) {
                push_color = Gui::ColorCyan;
            } else if (strcmp(tag_str, "magenta") == 0) {
                push_color = Gui::ColorMagenta;
            } else if (strcmp(tag_str, "white") == 0) {
                push_color = Gui::ColorWhite;
            } else if (strcmp(tag_str, "yellow") == 0) {
                push_color = Gui::ColorYellow;
            } else if (strcmp(tag_str, "/red") == 0 || strcmp(tag_str, "/cyan") == 0 ||
                       strcmp(tag_str, "/violet") == 0 || strcmp(tag_str, "/white") == 0 ||
                       strcmp(tag_str, "/yellow") == 0) {
                pop_color = true;
                draw = true;
            } else if (strcmp(tag_str, "option") == 0) {
                const int option_index = option_start + option_count;
                const OptionData &opt = text_options_[option_index];

                if (option_index == expanded_option_) {
                    push_color = Gui::ColorCyan;
                }
                if (opt.is_pressed) {
                    push_color = Gui::ColorGreen;
                } else if (opt.is_hover) {
                    push_color = Gui::ColorRed;
                } else {
                    push_color = Gui::ColorCyan;
                }

                rect_t &rect = out_options_rects.emplace_back();
                rect.dims[0] = draw_offset;
                rect.data = option_index;
            } else if (strcmp(tag_str, "/option") == 0) {
                pop_color = true;
                draw = true;

                const int option_index = option_start + option_count;
                const OptionData &opt = text_options_[option_index];

                // null terminate
                portion_buf[portion_buf_len] = '\0';

                const float width = font_.GetWidth(portion_buf, -1, this);

                rect_t &rect = out_options_rects.back();
                rect.dims[1] = Ren::Vec2f{width, font_height};

                ++option_count;
            } else if (strcmp(tag_str, "hint") == 0) {
                const int hint_index = hint_start + hint_count;
                const HintData &hint = text_hints_[hint_index];

                if (hint.is_hover) {
                    push_color = Gui::ColorGreen;
                } else {
                    push_color = Gui::ColorYellow;
                }

                rect_t &rect = out_hint_rects.emplace_back();
                rect.dims[0] = draw_offset;
                rect.data = hint_index;
            } else if (strcmp(tag_str, "/hint") == 0) {
                pop_color = true;
                draw = true;

                const int hint_index = hint_start + hint_count;
                const HintData &hint = text_hints_[hint_index];

                // null terminate
                portion_buf[portion_buf_len] = '\0';

                const float width = font_.GetWidth(portion_buf, -1, this);

                rect_t &rect = out_hint_rects.back();
                rect.dims[1] = Gui::Vec2f{width, font_height};

                ++hint_count;
            } else if (strcmp(tag_str, "split") == 0 || strcmp(tag_str, "/split") == 0) {
                // ignore
            } else {
                assert(false && "Unknown tag!");
            }

            if (push_color) {
                memcpy(text_color_stack[text_color_stack_size++], push_color, 4);
                draw = true;
            }

            // skip tag symbols
            char_start = char_pos;
        }

        for (int j = char_start; j < char_pos; j++) {
            portion_buf[portion_buf_len++] = text_data[j];
        }

        if (unicode == Gui::g_unicode_spacebar) {
            const int len_before = portion_buf_len;

            int next_pos = char_pos;
            while (text_data[next_pos] && next_pos < len) {
                const int next_start = next_pos;

                uint32_t unicode;
                next_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data[next_pos], unicode);

                // skip tag
                if (unicode == Gui::g_unicode_less_than) {
                    while (unicode != Gui::g_unicode_greater_than) {
                        next_pos += Gui::ConvChar_UTF8_to_Unicode(&text_data[next_pos], unicode);
                    }
                    continue;
                }

                for (int j = next_start; j < next_pos; j++) {
                    portion_buf[portion_buf_len++] = text_data[j];
                }

                if (unicode == Gui::g_unicode_spacebar) {
                    break;
                }
            }

            // null terminate
            portion_buf[portion_buf_len] = '\0';

            const float width = draw_offset[0] + font_.GetWidth(portion_buf, -1, this);
            if (width > 1.0f - side_margin) {
                new_line = true;
            }

            portion_buf_len = len_before;
            draw = true;
        }

        if (draw) {
            // null terminate
            portion_buf[portion_buf_len] = '\0';

            draw_offset[0] += font_.DrawText(r, portion_buf, draw_offset, cur_color, this);

            portion_buf_len = 0;
            if (new_line) {
                draw_offset[0] = dims_[0][0] + side_margin;
                draw_offset[1] -= font_height;
            }
            if (pop_color) {
                text_color_stack_size--;
            }
        }
    }

    // draw the last line
    if (portion_buf_len) {
        // null terminate
        portion_buf[portion_buf_len] = '\0';

        draw_offset[0] +=
            font_.DrawText(r, portion_buf, draw_offset, text_color_stack[text_color_stack_size - 1], this);
    }

    return draw_offset;
}
