#include "BaseElement.h"

#include "Renderer.h"

namespace Gui {
const Bitmask<eFlags> DefaultFlags = Bitmask<eFlags>(eFlags::Visible) | eFlags::Resizable;
} // namespace Gui

Gui::BaseElement::BaseElement(const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : flags_(DefaultFlags), parent_(parent) {
    if (parent) {
        parent->AddChild(this);
        BaseElement::Resize(pos, size);
    } else {
        rel_dims_[0] = pos;
        rel_dims_[1] = size;
    }
}

Gui::BaseElement::~BaseElement() {
    if (parent_) {
        parent_->RemoveChild(this);
    }
}

void Gui::BaseElement::Resize() {
    if (parent_) {
        dims_[0] = parent_->pos() + 0.5f * (rel_dims_[0] + Vec2f(1)) * parent_->size();
        dims_[1] = 0.5f * rel_dims_[1] * parent_->size();

        dims_px_[0] = Vec2i(Vec2f{parent_->pos_px()} + (0.5f * rel_dims_[0] + 0.5f) * Vec2f{parent_->size_px()});
        dims_px_[1] = Vec2i(rel_dims_[1] * Vec2f(parent_->size_px()) * 0.5f);
    }

    for (BaseElement *el : children_) {
        el->Resize();
    }
}

void Gui::BaseElement::Resize(const Vec2f &pos, const Vec2f &size) {
    rel_dims_[0] = pos;
    rel_dims_[1] = size;

    Resize();
}

bool Gui::BaseElement::Check(const Vec2i &p) const {
    return (p[0] >= dims_px_[0][0] && p[1] >= dims_px_[0][1] && p[0] <= dims_px_[0][0] + dims_px_[1][0] &&
            p[1] <= dims_px_[0][1] + dims_px_[1][1]);
}

bool Gui::BaseElement::Check(const Vec2f &p) const {
    return (p[0] >= dims_[0][0] && p[1] >= dims_[0][1] && p[0] <= dims_[0][0] + dims_[1][0] &&
            p[1] <= dims_[0][1] + dims_[1][1]);
}

bool Gui::BaseElement::HandleInput(const input_event_t &ev, const std::vector<bool> &keys_state) {
    for (BaseElement *el : children_) {
        if (el->HandleInput(ev, keys_state)) {
            return true;
        }
    }
    return false;
}

void Gui::BaseElement::Draw(Renderer *r) {
    r->PushClipArea(dims_);
    for (BaseElement *el : children_) {
        el->Draw(r);
    }
    r->PopClipArea();
}

void Gui::BaseElement::AddChild(BaseElement *el) const {
    const auto it = std::lower_bound(begin(children_), end(children_), el);
    if (it == std::end(children_) || el < (*it)) {
        children_.insert(it, el);
    }
}

void Gui::BaseElement::RemoveChild(BaseElement *el) const {
    const auto it = std::lower_bound(begin(children_), end(children_), el);
    if (it != end(children_) && el == (*it)) {
        children_.erase(it);
    }
}

Gui::Vec2f Gui::BaseElement::SnapToPixels(const Vec2f &p, const eSnapMode mode) const {
    float x, y;
    if (mode == eSnapMode::Down) {
        x = std::floor((0.5f + 0.5f * p[0]) * dims_px_[1][0]);
        y = std::floor((0.5f + 0.5f * p[1]) * dims_px_[1][1]);
    } else if (mode == eSnapMode::Up) {
        x = std::ceil((0.5f + 0.5f * p[0]) * dims_px_[1][0]);
        y = std::ceil((0.5f + 0.5f * p[1]) * dims_px_[1][1]);
    } else {
        x = std::round((0.5f + 0.5f * p[0]) * dims_px_[1][0]);
        y = std::round((0.5f + 0.5f * p[1]) * dims_px_[1][1]);
    }
    return 2 * Vec2f{x / float(dims_px_[1][0]), y / float(dims_px_[1][1])} - 1;
}