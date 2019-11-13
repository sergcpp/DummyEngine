#include "BaseElement.h"

namespace Gui {
namespace BaseElementConstants {
const unsigned default_flags = (1u << Visible) | (1u << Resizable);
}
}

Gui::BaseElement::BaseElement(const Vec2f &pos, const Vec2f &size, const BaseElement *parent)
    : flags_(BaseElementConstants::default_flags) {
    if (parent) {
        BaseElement::Resize(pos, size, parent);
    } else {
        rel_dims_[0] = pos;
        rel_dims_[1] = size;
    }
}

void Gui::BaseElement::Resize(const BaseElement *parent) {
    dims_[0] = parent->pos() + 0.5f * (rel_dims_[0] + Vec2f(1, 1)) * parent->size();
    dims_[1] = 0.5f * rel_dims_[1] * parent->size();

    dims_px_[0] = (Vec2i)(Vec2f{ parent->pos_px() } +0.5f * (rel_dims_[0] + Vec2f(1, 1)) * Vec2f { parent->size_px() });
    dims_px_[1] = (Vec2i)(rel_dims_[1] * (Vec2f)parent->size_px() * 0.5f);
}

void Gui::BaseElement::Resize(const Vec2f &pos, const Vec2f &size, const BaseElement *parent) {
    rel_dims_[0] = pos;
    rel_dims_[1] = size;

    Resize(parent);
}

bool Gui::BaseElement::Check(const Vec2i &p) const {
    return (p[0] >= dims_px_[0][0] &&
            p[1] >= dims_px_[0][1] &&
            p[0] <= dims_px_[0][0] + dims_px_[1][0] &&
            p[1] <= dims_px_[0][1] + dims_px_[1][1]);
}

bool Gui::BaseElement::Check(const Vec2f &p) const {
    return (p[0] >= dims_[0][0] &&
            p[1] >= dims_[0][1] &&
            p[0] <= dims_[0][0] + dims_[1][0] &&
            p[1] <= dims_[0][1] + dims_[1][1]);
}
