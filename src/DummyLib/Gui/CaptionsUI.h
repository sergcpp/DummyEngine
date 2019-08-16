#pragma once

#include <Eng/Gui/BaseElement.h>

namespace Gui {
class BitmapFont;
}

class CaptionsUI : public Gui::BaseElement {
    Gui::BitmapFont &font_;

    struct SeqCaption {
        const char *text;
        uint8_t color[4];
    };
    SeqCaption captions_[16] = {};
    int captions_count_ = 0;

  public:
    CaptionsUI(const Ren::Vec2f &pos, const Ren::Vec2f &size, Gui::BaseElement *parent, Gui::BitmapFont &font);

    void Draw(Gui::Renderer *r) override;

    void Clear();

    void OnPushCaption(const char *text, const uint8_t color[4]);
};