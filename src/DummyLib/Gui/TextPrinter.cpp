#include "TextPrinter.h"

#include <random>

#include <Eng/Gui/BitmapFont.h>
#include <Eng/Gui/Image9Patch.h>
#include <Eng/Gui/Utils.h>
#include <Ren/Context.h>
#include <Sys/Json.h>

namespace TextPrinterInternal {
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
}

TextPrinter::TextPrinter(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent, const std::shared_ptr<Gui::BitmapFont> &font)
    : Gui::BaseElement(pos, size, parent), parent_(parent), font_(font), data_pos_(0), progress_(0), expanded_option_(-1) {
    using namespace TextPrinterInternal;

    log_ = ctx.log();

    background_small_.reset(new Gui::Image9Patch{ ctx, Frame01, Ren::Vec2f{ 3.0f, 3.0f }, 1.0f,
                                                  Ren::Vec2f{ 0.0f, 0.0f }, Ren::Vec2f{ 1.0f, 1.0f }, parent });
    background_large_.reset(new Gui::Image9Patch{ ctx, Frame02, Ren::Vec2f{ 20.0f, 20.0f }, 1.0f,
                                                  Ren::Vec2f{ -1.0f, -1.0f }, Ren::Vec2f{ 2.0f, 2.0f }, this });
}

bool TextPrinter::LoadScript(const JsObject &js_script) {
    if (!js_script.Has("texts")) return false;

    text_data_.clear();
    text_options_.clear();
    option_variants_.clear();

    std::mt19937 rand_gen(std::random_device{}());

    try {
        const JsArray &js_texts = js_script.at("texts").as_arr();
        for (const JsElement &js_text_el : js_texts.elements) {
            const JsObject &js_text = js_text_el.as_obj();
            if (!js_text.Has("data")) return false;

            const JsString &js_data = js_text.at("data").as_str();
            text_data_.push_back(js_data.val);

            if (js_text.Has("options")) {
                text_options_.emplace_back();
                std::vector<OptionData> &text_option = text_options_.back();

                const JsArray &js_options = js_text.at("options").as_arr();
                for (const JsElement &js_opt_el : js_options.elements) {
                    const JsObject &js_opt = js_opt_el.as_obj();

                    text_option.emplace_back();

                    OptionData &opt_data = text_option.back();
                    opt_data.var_start = (int)option_variants_.size();
                    opt_data.var_count = 0;
                    opt_data.var_correct = opt_data.var_start;
                    opt_data.var_selected = -1;
                    opt_data.is_hover = false;
                    opt_data.is_expanded = false;

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

            if (js_text.Has("hints")) {
                text_hints_.emplace_back();

                const JsArray &js_hints = js_text.at("hints").as_arr();
                for (const JsElement &js_hint_el : js_hints.elements) {
                    const JsObject &js_hint = js_hint_el.as_obj();

                    text_hints_.back().emplace_back();

                    HintData &hint_data = text_hints_.back().back();
                    hint_data.str_index = (int)hint_strings_.size();
                    hint_data.is_hover = false;

                    const JsString &js_hint_en = js_hint.at("en").as_str();
                    hint_strings_.push_back(js_hint_en.val);
                }
            }
        }
    } catch (...) {
        log_->Error("TextPrinter::LoadScript Invalid JSON file!");
    }

    {   // determine length, parse options
        const std::string &cur_text_data = text_data_[data_pos_];
        orig_max_progress_ = 0;

        int option_index = 0, hint_index = 0;

        int char_pos = 0;
        while (cur_text_data[char_pos]) {
            uint32_t unicode;
            char_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data[char_pos], unicode);

            // skip tag
            if (unicode == Gui::g_unicode_less_than) {
                char tag_str[32];
                int tag_str_len = 0;

                while (unicode != Gui::g_unicode_greater_than) {
                    char_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data[char_pos], unicode);
                    tag_str[tag_str_len++] = (char)unicode;
                }
                tag_str[tag_str_len - 1] = '\0';

                if (strcmp(tag_str, "option") == 0) {
                    text_options_[data_pos_][option_index].pos = char_pos;
                } else if (strcmp(tag_str, "/option") == 0) {
                    text_options_[data_pos_][option_index].len = char_pos - 9 - text_options_[data_pos_][option_index].pos;
                    option_index++;
                } else if (strcmp(tag_str, "hint") == 0) {
                    text_hints_[data_pos_][hint_index].pos = char_pos;
                } else if (strcmp(tag_str, "/hint") == 0) {
                    text_hints_[data_pos_][hint_index].len = char_pos - 7 - text_hints_[data_pos_][hint_index].pos;
                    hint_index++;
                }

                continue;
            }

            ++orig_max_progress_;
        }

        max_progress_ = orig_max_progress_;
    }

    return false;
}

void TextPrinter::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);

    background_large_->Resize(this);
}

void TextPrinter::Draw(Gui::Renderer *r) {
    // TODO: dot not do this every frame
    UpdateTextBuffer();
    DrawTextBuffer(r);
}

void TextPrinter::Focus(const Gui::Vec2f &p) {
    BaseElement::Focus(p);

    for (OptionData &opt : text_options_[data_pos_]) {
        opt.is_hover = false;
    }

    for (HintData &hint : text_hints_[data_pos_]) {
        hint.is_hover = false;
    }

    hover_var_ = -1;
    if (expanded_option_ != -1) {
        for (int i = 0; i < (int)expanded_rects_.size(); i++) {
            const rect_t &rect = expanded_rects_[i];

            if (p[0] >= rect.dims[0][0] &&
                p[1] >= rect.dims[0][1] &&
                p[0] <= rect.dims[0][0] + rect.dims[1][0] &&
                p[1] <= rect.dims[0][1] + rect.dims[1][1]) {
                hover_var_ = i;
            }
        }

        return;
    }

    for (int i = 0; i < (int)options_rects_.size(); i++) {
        const rect_t &rect = options_rects_[i];
        OptionData &opt = text_options_[data_pos_][i];

        if (p[0] >= rect.dims[0][0] &&
            p[1] >= rect.dims[0][1] &&
            p[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            p[1] <= rect.dims[0][1] + rect.dims[1][1]) {
            opt.is_hover = true;
        }
    }

    for (int i = 0; i < (int)hint_rects_.size(); i++) {
        const rect_t &rect = hint_rects_[i];
        HintData &opt = text_hints_[data_pos_][i];

        if (p[0] >= rect.dims[0][0] &&
            p[1] >= rect.dims[0][1] &&
            p[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            p[1] <= rect.dims[0][1] + rect.dims[1][1]) {
            opt.is_hover = true;
        }
    }
}

void TextPrinter::Press(const Gui::Vec2f &p, bool push) {
    BaseElement::Press(p, push);

    if (progress_ != max_progress_) return;

    /*for (OptionData &opt : text_options_[data_pos_]) {
        opt.is_expanded = false;
        opt.is_pressed = false;
    }*/

    if (expanded_option_ != -1) {
        OptionData &opt = text_options_[data_pos_][expanded_option_];

        for (int i = 0; i < (int)expanded_rects_.size(); i++) {
            const rect_t &rect = expanded_rects_[i];

            if (p[0] >= rect.dims[0][0] &&
                p[1] >= rect.dims[0][1] &&
                p[0] <= rect.dims[0][0] + rect.dims[1][0] &&
                p[1] <= rect.dims[0][1] + rect.dims[1][1]) {

                if (push) {
                    return;
                } else {
                    opt.var_selected = i;
                }
            }
        }
    }

    for (int i = 0; i < (int)options_rects_.size(); i++) {
        const rect_t &rect = options_rects_[i];
        OptionData &opt = text_options_[data_pos_][i];

        if (p[0] >= rect.dims[0][0] &&
            p[1] >= rect.dims[0][1] &&
            p[0] <= rect.dims[0][0] + rect.dims[1][0] &&
            p[1] <= rect.dims[0][1] + rect.dims[1][1]) {
            

            if (push) {
                opt.is_pressed = true;
            } else {
                if (opt.is_expanded) {
                    opt.is_expanded = false;
                } else if (opt.is_pressed) {
                    opt.is_expanded = true;
                }
            }
        } else {
            opt.is_pressed = false;
            opt.is_expanded = false;
        }
    }
}

void TextPrinter::UpdateTextBuffer() {
    // replace options
    max_progress_ = orig_max_progress_;

    cur_text_data_ = text_data_[data_pos_];
    for (auto it = text_options_[data_pos_].rbegin(); it != text_options_[data_pos_].rend(); ++it) {
        if (it->var_selected != -1) {
            const std::string &sel_var = option_variants_[it->var_start + it->var_selected];

            cur_text_data_.erase(it->pos, it->len);
            cur_text_data_.insert(it->pos, sel_var);

            max_progress_ += (int)sel_var.length() - it->len;
        }
    }
}

void TextPrinter::DrawTextBuffer(Gui::Renderer *r) {
    options_rects_.clear();
    hint_rects_.clear();
    expanded_rects_.clear();

    char portion_buf[4096];
    int portion_buf_size = 0;

    const float font_height = font_->height(parent_);
    const uint8_t
        color_white[4] = { 255, 255, 255, 255 },
        color_red[4] = { 255, 0, 0, 255 },
        color_green[4] = { 0, 255, 0, 255 },
        color_cyan[4] = { 0, 255, 255, 255 },
        color_violet[4] = { 255, 0, 255, 255 },
        color_yellow[4] = { 255, 255, 0, 255 };

    // draw backdrop
    background_large_->Draw(r);

    uint8_t text_color_stack[32][4] = {
        { 255, 255, 255, 255 }
    };
    int text_color_stack_size = 1;

    const float side_offset = 16.0f / dims_px_[1][0];
    float x_offset = dims_[0][0] + side_offset, y_offset = dims_[0][1] + dims_[1][1] - font_height;

    expanded_option_ = -1;
    float expanded_x, expanded_y;

    int expanded_hint = -1;
    float expanded_hint_x, expanded_hint_y;

    int char_pos = 0;
    for (int i = 0; i < progress_; i++) {
        if (!cur_text_data_[char_pos]) break;
        int char_start = char_pos;

        uint32_t unicode;
        char_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data_[char_pos], unicode);

        const uint8_t *cur_color = text_color_stack[text_color_stack_size - 1];

        bool draw = false, new_line = false, pop_color = false;

        // parse tag
        if (unicode == Gui::g_unicode_less_than) {
            --i;

            char tag_str[32];
            int tag_str_len = 0;

            while (unicode != Gui::g_unicode_greater_than) {
                char_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data_[char_pos], unicode);
                tag_str[tag_str_len++] = (char)unicode;
            }
            tag_str[tag_str_len - 1] = '\0';

            const uint8_t *push_color = nullptr;

            if (strcmp(tag_str, "red") == 0) {
                push_color = color_red;
            } else if (strcmp(tag_str, "cyan") == 0) {
                push_color = color_cyan;
            } else if (strcmp(tag_str, "violet") == 0) {
                push_color = color_violet;
            } else if (strcmp(tag_str, "white") == 0) {
                push_color = color_white;
            } else if (strcmp(tag_str, "yellow") == 0) {
                push_color = color_yellow;
            } else if (strcmp(tag_str, "/red") == 0 || strcmp(tag_str, "/cyan") == 0 || strcmp(tag_str, "/violet") == 0 ||
                       strcmp(tag_str, "/white") == 0 || strcmp(tag_str, "/yellow") == 0) {
                pop_color = true;
                draw = true;
            } else if (strcmp(tag_str, "option") == 0) {
                const int option_index = (int)options_rects_.size();
                const OptionData &opt = text_options_[data_pos_][option_index];

                if (opt.is_expanded) {
                    push_color = color_cyan;
                } if (opt.is_pressed) {
                    push_color = color_green;
                } else if (opt.is_hover) {
                    push_color = color_red;
                } else {
                    push_color = color_cyan;
                }

                options_rects_.emplace_back();
                rect_t &rect = options_rects_.back();
                rect.dims[0] = Ren::Vec2f{ x_offset, y_offset };
            } else if (strcmp(tag_str, "/option") == 0) {
                pop_color = true;
                draw = true;

                const int option_index = (int)options_rects_.size() - 1;
                const OptionData &opt = text_options_[data_pos_][option_index];

                if (opt.is_expanded) {
                    expanded_option_ = option_index;
                    expanded_x = x_offset;
                    expanded_y = y_offset;
                }

                // null terminate
                portion_buf[portion_buf_size] = '\0';

                const float width = font_->GetWidth(portion_buf, -1, parent_);

                rect_t &rect = options_rects_.back();
                rect.dims[1] = Ren::Vec2f{ width, font_height };
            } else if (strcmp(tag_str, "hint") == 0) {
                const int hint_index = (int)hint_rects_.size();
                const HintData &hint = text_hints_[data_pos_][hint_index];

                if (hint.is_hover) {
                    push_color = color_green;
                } else {
                    push_color = color_yellow;
                }

                hint_rects_.emplace_back();
                rect_t &rect = hint_rects_.back();
                rect.dims[0] = Ren::Vec2f{ x_offset, y_offset };
            } else if (strcmp(tag_str, "/hint") == 0) {
                pop_color = true;
                draw = true;

                const int hint_index = (int)hint_rects_.size() - 1;
                const HintData &hint = text_hints_[data_pos_][hint_index];

                if (hint.is_hover) {
                    expanded_hint = hint_index;
                    expanded_hint_x = x_offset;
                    expanded_hint_y = y_offset;
                }

                // null terminate
                portion_buf[portion_buf_size] = '\0';

                const float width = font_->GetWidth(portion_buf, -1, parent_);

                rect_t &rect = hint_rects_.back();
                rect.dims[1] = Gui::Vec2f{ width, font_height };
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
            portion_buf[portion_buf_size++] = cur_text_data_[j];
        }

        if (unicode == Gui::g_unicode_spacebar) {
            const int len_before = portion_buf_size;

            int next_pos = char_pos;
            while (cur_text_data_[next_pos]) {
                const int next_start = next_pos;

                uint32_t unicode;
                next_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data_[next_pos], unicode);

                // skip tag
                if (unicode == Gui::g_unicode_less_than) {
                    while (unicode != Gui::g_unicode_greater_than) {
                        next_pos += Gui::ConvChar_UTF8_to_Unicode(&cur_text_data_[next_pos], unicode);
                    }
                    continue;
                }

                for (int j = next_start; j < next_pos; j++) {
                    portion_buf[portion_buf_size++] = cur_text_data_[j];
                }

                if (unicode == Gui::g_unicode_spacebar) break;
            }

            // null terminate
            portion_buf[portion_buf_size] = '\0';

            const float width = x_offset + font_->GetWidth(portion_buf, -1, parent_);
            if (width > 1.0f - side_offset) {
                new_line = true;
            }

            portion_buf_size = len_before;
            draw = true;
        }

        if (draw) {
            // null terminate
            portion_buf[portion_buf_size] = '\0';

            x_offset += font_->DrawText(r, portion_buf, Gui::Vec2f{ x_offset, y_offset }, cur_color, parent_);

            portion_buf_size = 0;
            if (new_line) {
                y_offset -= font_height;
                x_offset = dims_[0][0] + side_offset;
            }
            if (pop_color) {
                text_color_stack_size--;
            }
        }
    }

    // draw the last line
    if (portion_buf_size) {
        // null terminate
        portion_buf[portion_buf_size] = '\0';

        x_offset += font_->DrawText(r, portion_buf, Gui::Vec2f{ x_offset, y_offset }, text_color_stack[text_color_stack_size - 1], parent_);
    }

    // draw option selector
    if (expanded_option_ != -1) {
        const OptionData &opt = text_options_[data_pos_][expanded_option_];
        assert(opt.is_expanded);

        expanded_y += font_height;
        auto
            exp_back_pos = Gui::Vec2f{ expanded_x - 0.1f * font_height, expanded_y - 0.25f * font_height },
            exp_back_size = Gui::Vec2f{ 0.0f, 0.25f * font_height };

        for (int i = 0; i < opt.var_count; i++) {
            const std::string &var = option_variants_[opt.var_start + i];

            float width = font_->GetWidth(var.c_str(), -1, parent_);
            exp_back_size[0] = std::max(exp_back_size[0], width);
            exp_back_size[1] += font_height;
        }

        exp_back_size[0] += 0.2f * font_height;

        background_small_->Resize(exp_back_pos, exp_back_size, parent_);
        background_small_->Draw(r);

        for (int i = 0; i < opt.var_count; i++) {
            const std::string &var = option_variants_[opt.var_start + i];

            float width = font_->DrawText(r, var.c_str(), Gui::Vec2f{ expanded_x, expanded_y }, (i == hover_var_) ? color_red : color_white, parent_);

            expanded_rects_.emplace_back();
            rect_t &rect = expanded_rects_.back();

            rect.dims[0] = Gui::Vec2f{ expanded_x,    expanded_y };
            rect.dims[1] = Gui::Vec2f{ width,         font_height * 0.8f };

            expanded_y += font_height;
        }
    }

    // draw translated hint
    if (expanded_hint != -1) {
        const HintData &opt = text_hints_[data_pos_][expanded_hint];
        assert(opt.is_hover);

        float width = font_->GetWidth(hint_strings_[opt.str_index].c_str(), -1, parent_);

        expanded_hint_y += font_height;

        background_small_->Resize(Gui::Vec2f{ expanded_hint_x - 0.1f * font_height, expanded_hint_y - 0.25f * font_height },
                                  Gui::Vec2f{ width + 0.2f * font_height, 1.25 * font_height }, parent_);
        background_small_->Draw(r);

        font_->DrawText(r, hint_strings_[opt.str_index].c_str(), Gui::Vec2f{ expanded_hint_x, expanded_hint_y }, color_white, parent_);
    }
}