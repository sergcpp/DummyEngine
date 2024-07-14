#pragma once

#include <functional>
#include <string_view>

#include <Eng/gui/BaseElement.h>
#include <Sys/Signal_.h>

class DialogUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;
    bool debug_;

    struct {
        std::string_view key;
        std::string_view text;
        int off;
    } choices_[8] = {};
    int choices_count_ = 0;

    int hovered_choice_ = -1, clicked_choice_ = -1;

    void IterateChoices(const std::function<bool(int i, const Ren::Vec2f &pos, const Ren::Vec2f &size)> &callback);

  public:
    DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent, const Gui::BitmapFont &font,
             bool debug = false);

    void Draw(Gui::Renderer *r) override;

    void Clear();

    void Resize(const Gui::BaseElement *parent) override;
    using BaseElement::Resize;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Hover(const Ren::Vec2f &p) override;

    void OnPushChoice(std::string_view key, std::string_view text, int off);

    Sys::SignalN<void(std::string_view key)> make_choice_signal;
};
