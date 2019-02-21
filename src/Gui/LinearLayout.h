#pragma once

#include <vector>

#include "BaseElement.h"

namespace Gui {
class LinearLayout : public BaseElement {
protected:
    std::vector<BaseElement *>  elements_;
    bool                        vertical_;
public:
    LinearLayout(const Vec2f &start, const Vec2f &size, const BaseElement *parent) :
        BaseElement(start, size, parent), vertical_(false) {
    }

    bool vertical() const {
        return vertical_;
    }

    void set_vetical(bool v) {
        vertical_ = v;
    }

    template<class T>
    T *AddElement(T *el) {
        elements_.push_back(el);
        return el;
    }

    template<class T>
    T *InsertElement(T *el, size_t pos) {
        elements_.insert(elements_.begin() + pos, el);
        return el;
    }

    template<class T>
    T *ReplaceElement(T *el, size_t pos) {
        if (pos == elements_.size()) {
            elements_.push_back(el);
        } else {
            elements_[pos] = el;
        }
    }

    void Clear() {
        elements_.clear();
    }

    void Resize(const BaseElement *parent) override;
    void Resize(const Vec2f &start, const Vec2f &size, const BaseElement *parent) override;

    bool Check(const Vec2i &p) const override;
    bool Check(const Vec2f &p) const override;

    void Focus(const Vec2i &p) override;
    void Focus(const Vec2f &p) override;

    void Press(const Vec2i &p, bool push) override;
    void Press(const Vec2f &p, bool push) override;

    void Draw(Renderer *r) override;
};
}
