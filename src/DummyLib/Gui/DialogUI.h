#pragma once

#include <Eng/Gui/BaseElement.h>
#include <Sys/Signal_.h>

class DialogUI : public Gui::BaseElement {
    Gui::BitmapFont &font_;

    struct {
        const char *key;
        const char *text;
    } choices_[8];
    int choices_count_ = 0;

    int hovered_choice_ = -1, clicked_choice_ = -1;

    void IterateChoices(
        std::function<bool(int i, const Ren::Vec2f &pos, const Ren::Vec2f &size)>
            callback);

    Ren::Vec2f ToLocal(const Ren::Vec2f &p);

  public:
    DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
             Gui::BitmapFont &font);

    void Draw(Gui::Renderer *r) override;

    void Clear();

    void Resize(const Gui::BaseElement *parent) override;
    using BaseElement::Resize;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Hover(const Ren::Vec2f &p) override;

    void OnPushChoice(const char *key, const char *text);

    Sys::Signal<void(const char *key)> make_choice_signal;
};
