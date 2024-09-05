#include "CaptionsUI.h"

#include <Gui/BitmapFont.h>

CaptionsUI::CaptionsUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, Gui::BaseElement *parent,
                       const Gui::BitmapFont &font)
    : Gui::BaseElement(pos, size, parent), font_(font) {}

void CaptionsUI::Draw(Gui::Renderer *r) {
    const float font_height = font_.height(this);
    float y_coord = -0.5f;

    for (int i = captions_count_ - 1; i >= 0; i--) {
        const SeqCaption &cap = captions_[i];

        const float width = font_.GetWidth(cap.text, this);

        font_.DrawText(r, cap.text, Gui::Vec2f{-0.5f * width, y_coord}, cap.color, this);
        y_coord += font_height;
    }
}

void CaptionsUI::Clear() { captions_count_ = 0; }

void CaptionsUI::OnPushCaption(std::string_view text, const uint8_t color[4]) {
    SeqCaption &cap = captions_[captions_count_++];
    cap.text = text;
    memcpy(cap.color, color, 4);
}