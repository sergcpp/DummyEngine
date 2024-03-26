#include "DialogUI.h"

#include <Eng/gui/BitmapFont.h>
#include <Eng/gui/Renderer.h>

DialogUI::DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                   const Gui::BitmapFont &font, const bool debug)
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
            snprintf(buf, sizeof(buf), "%s [%s]", choices_[i].text, choices_[i].key);
            font_.DrawText(r, buf, pos, col, this);
        }
        return true;
    });
}

void DialogUI::Clear() { choices_count_ = 0; }

void DialogUI::Resize(const Gui::BaseElement *parent) { BaseElement::Resize(parent); }

void DialogUI::Press(const Ren::Vec2f &p, const bool push) {
    clicked_choice_ = -1;
    if (Check(p)) {
        const Ren::Vec2f lp = ToLocal(p);
        IterateChoices([&](int i, const Ren::Vec2f &pos, const Ren::Vec2f &size) {
            if (lp[0] > pos[0] && lp[1] > pos[1] && lp[0] < pos[0] + size[0] && lp[1] < pos[1] + size[1]) {
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
            if (lp[0] > pos[0] && lp[1] > pos[1] && lp[0] < pos[0] + size[0] && lp[1] < pos[1] + size[1]) {
                hovered_choice_ = i;
                return false;
            }
            return true;
        });
    }
}

void DialogUI::IterateChoices(
    const std::function<bool(int i, const Ren::Vec2f &pos, const Ren::Vec2f &size)> &callback) {
    const float font_height = font_.height(this);
    float y_coord = -0.5f;

    for (int i = 0; i < choices_count_; i++) {
        const float width = font_.GetWidth(choices_[i].text, this);
        if (!callback(i, Ren::Vec2f{-0.5f * width + 0.5f * float(choices_->off), y_coord},
                      Ren::Vec2f{width, font_height})) {
            break;
        }
        y_coord -= font_height;
    }
}

void DialogUI::OnPushChoice(const char *key, const char *text, const int off) {
    choices_[choices_count_] = {key, text, off};
    ++choices_count_;
}