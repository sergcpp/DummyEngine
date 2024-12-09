#pragma once

#include <Gui/Image9Patch.h>
#include <Gui/Signal.h>

namespace Sys {
template <typename Alloc> struct JsObjectT;
using JsObject = JsObjectT<std::allocator<char>>;
} // namespace Sys

class WordPuzzleUI : public Gui::BaseElement {
    struct OptionData {
        int pos, len;
        int var_start, var_count, var_correct;

        // temp data
        int var_selected;
        bool is_hover, is_pressed;
    };

    struct HintData {
        int pos, len;
        int str_index;
        bool is_hover;
    };

    struct SplitData {
        int pos, len;
        int hint_start;
        int option_start;
        int slot_index;
    };

    enum class eState { AnimIntro, Building, Correcting, AnimOutro, Solved };

    Ren::ILog *log_;
    const Gui::BitmapFont &font_;
    eState state_ = eState::Solved;
    Gui::Image9Patch background_small_, background_large_;
    std::string text_data_;
    std::vector<SplitData> text_splits_;
    std::vector<OptionData> text_options_;
    std::vector<HintData> text_hints_;
    std::vector<std::string> option_variants_;
    std::vector<std::string> hint_strings_;
    std::string preprocessed_text_data_;
    int expanded_option_ = -1, expanded_hint_ = -1;

    struct rect_t {
        Gui::Vec2f dims[2];
        int data = -1;
    };
    std::vector<rect_t> options_rects_, hint_rects_, expanded_rects_, split_rects_;
    std::vector<int> avail_splits_, chosen_splits_;

    double anim_started_time_s_ = 0.0;

    int hover_var_;

    void UpdateTextBuffer();
    void UpdateState(double cur_time_s);

    Gui::Vec2f DrawTextBuffer(Gui::Renderer *r, std::string_view text_data, Gui::Vec2f draw_offset,
                              std::vector<rect_t> &out_options_rects, int option_start,
                              std::vector<rect_t> &out_hint_rects, int hint_start);

  public:
    WordPuzzleUI(Ren::Context &ctx, const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                 const Gui::BitmapFont &font);

    bool active() const { return state_ != eState::Solved; }

    void Cancel();
    void Restart();

    bool Load(const Sys::JsObject &js_puzzle);

    void Resize() override;

    void Draw(Gui::Renderer *r) override;

    //void Hover(const Gui::Vec2f &p) override;
    //void Press(const Gui::Vec2f &p, bool push) override;

    Gui::SignalN<void()> puzzle_solved_signal;
};
