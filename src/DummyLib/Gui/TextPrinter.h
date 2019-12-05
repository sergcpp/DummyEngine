#pragma once

#include <Gui/BaseElement.h>

class TextPrinter : public Gui::BaseElement {
    struct OptionData {
        int pos, len;
        int var_start, var_count, var_correct;

        // temp data
        int var_selected;
        bool is_hover, is_expanded, is_pressed;
    };

    struct HintData {
        int pos, len;
        int str_index;
        bool is_hover;
    };

    const Gui::BaseElement              *parent_;
    std::shared_ptr<Gui::BitmapFont>    font_;
    std::unique_ptr<Gui::ImageNinePatch>background_small_, background_large_;
    std::vector<std::string>            text_data_;
    std::vector<std::vector<OptionData>>text_options_;
    std::vector<std::vector<HintData>>  text_hints_;
    std::vector<std::string>            option_variants_;
    std::vector<std::string>            hint_strings_;
    std::string                         cur_text_data_;
    int                                 data_pos_, progress_, orig_max_progress_, max_progress_;
    int                                 expanded_option_;

    struct rect_t {
        Gui::Vec2f dims[2];
    };
    std::vector<rect_t>                 options_rects_, hint_rects_, expanded_rects_;

    int                                 hover_var_;

    void UpdateTextBuffer();
    void DrawTextBuffer(Gui::Renderer *r);
public:
    TextPrinter(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent, const std::shared_ptr<Gui::BitmapFont> &font);

    void restart() { progress_ = 0; }
    void skip() { progress_ = max_progress_; }
    void incr_progress() { progress_ = std::min(progress_ + 1, max_progress_); }

    bool LoadScript(const JsObject &js_script);

    void Resize(const BaseElement *parent) override;

    void Draw(Gui::Renderer *r) override;

    //void Focus(const Gui::Vec2i &p) override;
    void Focus(const Gui::Vec2f &p) override;

    //void Press(const Vec2i &/*p*/, bool /*push*/) override;
    void Press(const Gui::Vec2f &p, bool push) override;
};