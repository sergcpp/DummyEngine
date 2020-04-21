#include "DialogUI.h"

DialogUI::DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size,
                   const BaseElement *parent, Gui::BitmapFont &font, bool debug)
    : Gui::BaseElement(pos, size, parent), font_(font), debug_(debug) {}

void DialogUI::Draw(Gui::Renderer *r) {
    IterateChoices([&](int i, const Ren::Vec2f &pos, const Ren::Vec2f &size) {
        const uint8_t *col = (i == hovered_choice_) ? Gui::ColorCyan : Gui::ColorWhite;
        if (i == clicked_choice_) {
            col = Gui::ColorRed;
        }
        if (!debug_) {
            font_.DrawText(r, choices_[i].text, pos, col, this);
        } else {
            char buf[256];
            sprintf(buf, "%s [%s]", choices_[i].text, choices_[i].key);
            font_.DrawText(r, buf, pos, col, this);
        }
        return true;
    });
}

void DialogUI::Clear() { choices_count_ = 0; }

void DialogUI::Resize(const Gui::BaseElement *parent) { BaseElement::Resize(parent); }

void DialogUI::Press(const Ren::Vec2f &p, bool push) {
    clicked_choice_ = -1;
    if (Check(p)) {
        const Ren::Vec2f lp = ToLocal(p);
        IterateChoices([&](int i, const Ren::Vec2f &pos, const Ren::Vec2f &size) {
            if (lp[0] > pos[0] && lp[1] > pos[1] && lp[0] < pos[0] + size[0] &&
                lp[1] < pos[1] + size[1]) {
                if (push) {
                    clicked_choice_ = i;
                } else {
                    make_choice_signal.FireN(choices_[i].key);
                }
                return false;
            }
            return true;
        });
    }
}

void DialogUI::Hover(const Ren::Vec2f &p) {
    hovered_choice_ = -1;
    if (Check(p)) {
        const Ren::Vec2f lp = ToLocal(p);
        IterateChoices([&](int i, const Ren::Vec2f &pos, const Ren::Vec2f &size) {
            if (lp[0] > pos[0] && lp[1] > pos[1] && lp[0] < pos[0] + size[0] &&
                lp[1] < pos[1] + size[1]) {
                hovered_choice_ = i;
                return false;
            }
            return true;
        });
    }
}

void DialogUI::IterateChoices(
    std::function<bool(int i, const Ren::Vec2f &pos, const Ren::Vec2f &size)> callback) {
    const float font_height = font_.height(this);
    float y_coord = -0.5f;

    for (int i = 0; i < choices_count_; i++) {
        const float width = font_.GetWidth(choices_[i].text, -1, this);
        if (!callback(i, Ren::Vec2f{-0.5f * width, y_coord},
                      Ren::Vec2f{width, font_height})) {
            break;
        }
        y_coord -= font_height;
    }
}

void DialogUI::OnPushChoice(const char *key, const char *text) {
    choices_[choices_count_].key = key;
    choices_[choices_count_].text = text;
    ++choices_count_;
}