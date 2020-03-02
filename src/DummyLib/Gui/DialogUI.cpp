#include "DialogUI.h"

DialogUI::DialogUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent)
    : Gui::BaseElement(pos, size, parent) {

}

bool DialogUI::LoadSequence(const JsObject &js_seq) {
    return false;
}

void DialogUI::Draw(Gui::Renderer *r) {
    
}
