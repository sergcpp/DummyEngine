#include "ButtonBase.h"

Gui::ButtonBase::ButtonBase(const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent), state_(eState::Normal) {}

void Gui::ButtonBase::Hover(const Vec2i &p) {
    if (state_ != eState::Pressed) {
        if (Check(p)) {
            state_ = eState::Focused;
        } else {
            state_ = eState::Normal;
        }
    }
}

void Gui::ButtonBase::Hover(const Vec2f &p) {
    if (state_ != eState::Pressed) {
        if (Check(p)) {
            state_ = eState::Focused;
        } else {
            state_ = eState::Normal;
        }
    }
}

void Gui::ButtonBase::Press(const Vec2i &p, bool push) {
    if (state_ != eState::Normal) {
        if (Check(p)) {
            if (push) {
                state_ = eState::Pressed;
            } else {
                pressed_signal.FireN();
                state_ = eState::Focused;
            }
        } else {
            state_ = eState::Normal;
        }
    }
}

void Gui::ButtonBase::Press(const Vec2f &p, bool push) {
    if (state_ != eState::Normal) {
        if (Check(p)) {
            if (push) {
                state_ = eState::Pressed;
            } else {
                pressed_signal.FireN();
                state_ = eState::Focused;
            }
        } else {
            state_ = eState::Normal;
        }
    }
}