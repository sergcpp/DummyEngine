#include "SeqEditUI.h"

#include <Eng/Gui/BitmapFont.h>
#include <Eng/Gui/Image.h>

namespace SeqEditUIInternal {
// const char RULER_TEXTURE[] = "assets_pc/textures/ruler_small.uncompressed.tga";
}

SeqEditUI::SeqEditUI(Ren::Context &ctx, const Gui::BitmapFont &font,
                     const Gui::Vec2f &pos, const Gui::Vec2f &size,
                     const Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), parent_(parent),
      font_(font), timeline_{ctx, font, Gui::Vec2f{}, Gui::Vec2f{}, this},
      canvas_{ctx, font, Gui::Vec2f{}, Gui::Vec2f{}, this} {
    SeqEditUI::Resize(parent);

    timeline_.time_changed_signal.Connect<SeqCanvasUI, &SeqCanvasUI::OnCurTimeChange>(
        &canvas_);
    timeline_.set_time_cur(0.0f);
}

void SeqEditUI::Draw(Gui::Renderer *r) {
    timeline_.Draw(r);
    canvas_.Draw(r);
}

void SeqEditUI::Resize(const BaseElement *parent) {
    BaseElement::Resize(parent);

    const float timeline_height = 2.0f * 48.0f / float(dims_px_[1][1]);
    timeline_.Resize(Gui::Vec2f{-1.0f, 1.0f - timeline_height},
                     Gui::Vec2f{2.0f, timeline_height}, this);

    canvas_.Resize(Gui::Vec2f{-1.0f, -1.0f}, Gui::Vec2f{2.0f, 2.0f - timeline_height},
                   this);
}

void SeqEditUI::Press(const Ren::Vec2f &p, const bool push) {
    timeline_.Press(p, push);
    canvas_.Press(p, push);
}

void SeqEditUI::Hover(const Ren::Vec2f &p) {
    timeline_.Hover(p);
    canvas_.Hover(p);
}

void SeqEditUI::PressRMB(const Ren::Vec2f &p, const bool push) { timeline_.PressRMB(p, push); }