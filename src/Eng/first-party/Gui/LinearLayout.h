#pragma once

#include <vector>

#include "BaseElement.h"

namespace Gui {
class LinearLayout : public BaseElement {
  protected:
    std::vector<BaseElement *> elements_;
    bool vertical_;

  public:
    LinearLayout(const Vec2f &start, const Vec2f &size, const BaseElement *parent)
        : BaseElement(start, size, parent), vertical_(false) {}

    [[nodiscard]] bool vertical() const { return vertical_; }

    void set_vetical(bool v) { vertical_ = v; }

    template <class T> T *AddElement(T *el) {
        elements_.push_back(el);
        return el;
    }

    template <class T> T *InsertElement(T *el, const size_t pos) {
        elements_.insert(elements_.begin() + pos, el);
        return el;
    }

    template <class T> T *ReplaceElement(T *el, const size_t pos) {
        if (pos == elements_.size()) {
            elements_.push_back(el);
        } else {
            elements_[pos] = el;
        }
    }

    void Clear() { elements_.clear(); }

    void Resize() override;
    void Resize(const Vec2f &start, const Vec2f &size) override;

    [[nodiscard]] bool Check(const Vec2i &p) const override;
    [[nodiscard]] bool Check(const Vec2f &p) const override;

    void Draw(Renderer *r) override;
};
} // namespace Gui
