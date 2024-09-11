#pragma once

#include <Gui/Image9Patch.h>
#include <Gui/Signal.h>

class TimelineUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;
    Gui::Image9Patch back_;
    Gui::Image ruler_small_, ruler_medium_, ruler_big_;
    Gui::Image time_pos_;

    int time_scale_index_;
    float time_offset_;
    float time_cur_, time_step_;

    bool grabbed_, snap_to_grid_;
    bool grabbed_rmb_;
    float rmb_time_offset_;
    Gui::Vec2f rmb_point_;

    float GetTimeFromPoint(float px) const;
    float GetPointFromTime(float t) const;
    void SetCurTimeFromPoint(float px);

  public:
    TimelineUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
               const Gui::BaseElement *parent);

    bool grabbed() const { return grabbed_; }

    float time_cur() const { return time_cur_; }
    void set_time_cur(float time_s);

    Gui::Vec2f time_range() const;

    void ZoomIn();
    void ZoomOut();

    void Draw(Gui::Renderer *r) override;

    void Resize() override;
    using BaseElement::Resize;

    //void Press(const Gui::Vec2f &p, bool push) override;
    //void Hover(const Gui::Vec2f &p) override;

    void PressRMB(const Gui::Vec2f &p, bool push);

    Gui::SignalN<void(float, float, float)> time_changed_signal;
};
