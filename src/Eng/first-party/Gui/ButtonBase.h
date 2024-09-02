#pragma once

#include "BaseElement.h"
#include "Signal.h"

namespace Gui {
class ButtonBase : public BaseElement {
  protected:
    enum class eState { Normal, Focused, Pressed } state_;

  public:
    ButtonBase(const Vec2f &pos, const Vec2f &size, const BaseElement *parent);

    void Hover(const Vec2i &p) override;
    void Hover(const Vec2f &p) override;

    void Press(const Vec2i &p, bool push) override;
    void Press(const Vec2f &p, bool push) override;

    Signal<void()> pressed_signal;
};
} // namespace Gui
