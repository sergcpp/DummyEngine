#include "ButtonBase.h"

Gui::ButtonBase::ButtonBase(const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : BaseElement(pos, size, parent), state_(ST_NORMAL) {
}

void Gui::ButtonBase::Focus(const Vec2i &p) {
    if (state_ != ST_PRESSED) {
        if (Check(p)) {
            state_ = ST_FOCUSED;
        } else {
            state_ = ST_NORMAL;
        }
    }
}

void Gui::ButtonBase::Focus(const Vec2f &p) {
    if (state_ != ST_PRESSED) {
        if (Check(p)) {
            state_ = ST_FOCUSED;
        } else {
            state_ = ST_NORMAL;
        }
    }
}

void Gui::ButtonBase::Press(const Vec2i &p, bool push) {
    if (state_ != ST_NORMAL) {
        if (Check(p)) {
            if (push) {
                state_ = ST_PRESSED;
            } else {
                pressed_signal.FireN();
                state_ = ST_FOCUSED;
            }
        } else {
            state_ = ST_NORMAL;
        }
    }
}

void Gui::ButtonBase::Press(const Vec2f &p, bool push) {
    if (state_ != ST_NORMAL) {
        if (Check(p)) {
            if (push) {
                state_ = ST_PRESSED;
            } else {
                pressed_signal.FireN();
                state_ = ST_FOCUSED;
            }
        } else {
            state_ = ST_NORMAL;
        }
    }
}