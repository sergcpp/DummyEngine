#pragma once

#include <Eng/Gui/BaseElement.h>

class DialogUI : public Gui::BaseElement {
public:
    DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent);

    bool LoadSequence(const JsObject &js_seq);

    void Draw(Gui::Renderer *r) override;
};
