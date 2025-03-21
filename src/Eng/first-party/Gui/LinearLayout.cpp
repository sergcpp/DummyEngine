#include "LinearLayout.h"

void Gui::LinearLayout::Resize() {
    BaseElement::Resize();

    auto _start = Vec2f{-1}, _size = Vec2f{2};

    float spacing;
    float filled_space = 0;
    float l;
    if (vertical_) {
        spacing = 8.0f / float(parent_->size_px()[1]);
        l = _size[1] - spacing * float(elements_.size() + 1);
        for (BaseElement *el : elements_) {
            if (el->flags() & eFlags::Resizable) {
                filled_space += el->rel_size()[1];
            } else {
                l -= el->rel_size()[1];
            }
        }
    } else {
        spacing = 8.0f / float(parent_->size_px()[0]);
        l = _size[0] - spacing * float(elements_.size() + 1);
        for (BaseElement *el : elements_) {
            if (el->flags() & eFlags::Resizable) {
                filled_space += el->rel_size()[0];
            } else {
                l -= el->rel_size()[0];
            }
        }
    }

    float mult = 1;
    if (filled_space > 0) {
        mult = l / filled_space;
    }
    float pad = 0;

    if (vertical_) {
        pad = _start[1] + _size[1] - spacing;

        for (BaseElement *el : elements_) {
            el->Resize(Vec2f{_start[0], 1}, Vec2f{_size[0], el->rel_size()[1] * mult});
            pad -= (el->rel_size()[1] + spacing);
            el->Resize(Vec2f{_start[0], pad}, Vec2f{_size[0], el->rel_size()[1]});
        }
    } else {
        pad = _start[0] + spacing;
        for (BaseElement *el : elements_) {
            el->Resize(Vec2f{pad, _start[1]}, Vec2f{el->rel_size()[0] * mult, _size[1]});
            pad += el->rel_size()[0] + spacing;
        }
    }
}

void Gui::LinearLayout::Resize(const Vec2f &start, const Vec2f &size) {
    BaseElement::Resize(start, size);
}

bool Gui::LinearLayout::Check(const Vec2i &p) const {
    return std::any_of(begin(elements_), end(elements_), [p](const BaseElement *el) { return el->Check(p); });
}

bool Gui::LinearLayout::Check(const Vec2f &p) const {
    return std::any_of(begin(elements_), end(elements_), [p](const BaseElement *el) { return el->Check(p); });
}

void Gui::LinearLayout::Draw(Renderer *r) {
    for (BaseElement *el : elements_) {
        el->Draw(r);
    }
}