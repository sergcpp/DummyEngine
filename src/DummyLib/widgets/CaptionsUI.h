#pragma once

#include <string_view>

#include <Gui/BaseElement.h>

namespace Gui {
class BitmapFont;
}

class CaptionsUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;

    struct SeqCaption {
        std::string_view text;
        uint8_t color[4];
    };
    SeqCaption captions_[16] = {};
    int captions_count_ = 0;

  public:
    CaptionsUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, Gui::BaseElement *parent, const Gui::BitmapFont &font);

    void Draw(Gui::Renderer *r) override;

    void Clear();

    void OnPushCaption(std::string_view text, const uint8_t color[4]);
};