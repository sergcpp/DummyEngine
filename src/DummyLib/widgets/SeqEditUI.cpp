#include "SeqEditUI.h"

#include <Gui/BitmapFont.h>
#include <Gui/Image.h>

namespace SeqEditUIInternal {
// const char RULER_TEXTURE[] = "assets_pc/textures/ruler_small.uncompressed.tga";
}

SeqEditUI::SeqEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                     const Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), parent_(parent), font_(font),
      timeline_{ctx, font, Gui::Vec2f{}, Gui::Vec2f{}, this}, canvas_{ctx, font, Gui::Vec2f{}, Gui::Vec2f{}, this} {
    SeqEditUI::Resize();

    timeline_.time_changed_signal.Connect<SeqCanvasUI, &SeqCanvasUI::OnCurTimeChange>(&canvas_);
    timeline_.set_time_cur(0);
}

void SeqEditUI::Draw(Gui::Renderer *r) {
    timeline_.Draw(r);
    canvas_.Draw(r);
}

void SeqEditUI::Resize() {
    BaseElement::Resize();

    const float timeline_height = 2 * 48 / float(dims_px_[1][1]);
    timeline_.Resize(Gui::Vec2f{-1, 1 - timeline_height}, Gui::Vec2f{2, timeline_height});

    canvas_.Resize(Gui::Vec2f{-1}, Gui::Vec2f{2, 2 - timeline_height});
}

/*void SeqEditUI::Press(const Gui::Vec2f &p, const bool push) {
    timeline_.Press(p, push);
    canvas_.Press(p, push);
}*/

/*void SeqEditUI::Hover(const Gui::Vec2f &p) {
    timeline_.Hover(p);
    canvas_.Hover(p);
}*/

void SeqEditUI::PressRMB(const Gui::Vec2f &p, const bool push) { timeline_.PressRMB(p, push); }