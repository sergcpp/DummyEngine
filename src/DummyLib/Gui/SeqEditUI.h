#pragma once

#include <Eng/Gui/BaseElement.h>

#include "SeqCanvasUI.h"
#include "TimelineUI.h"

class SeqEditUI : public Gui::BaseElement {
    const Gui::BaseElement *parent_;
    const Gui::BitmapFont &font_;
    TimelineUI timeline_;
    SeqCanvasUI canvas_;

  public:
    SeqEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
              const Gui::BaseElement *parent);

    void set_sequence(ScriptedSequence *seq) { canvas_.set_sequence(seq); }

    void ZoomInTime() { timeline_.ZoomIn(); }
    void ZoomOutTime() { timeline_.ZoomOut(); }

    bool timeline_grabbed() const { return timeline_.grabbed(); }

    float GetTime() const { return timeline_.time_cur(); }
    void SetTime(const float time_s) { timeline_.set_time_cur(time_s); }

    void Draw(Gui::Renderer *r) override;

    void Resize(const BaseElement *parent) override;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Hover(const Ren::Vec2f &p) override;

    void PressRMB(const Ren::Vec2f &p, bool push);
};
