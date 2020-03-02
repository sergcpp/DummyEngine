#pragma once

#include <Eng/Gui/Image9Patch.h>
#include <Sys/Signal_.h>

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
    Ren::Vec2f rmb_point_;

    float GetTimeFromPoint(float px);
    float GetPointFromTime(float t);
    void SetCurTimeFromPoint(float px);
    Ren::Vec2f SnapToPixels(const Ren::Vec2f &p);

  public:
    TimelineUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Ren::Vec2f &pos,
               const Ren::Vec2f &size, const Gui::BaseElement *parent);

    bool grabbed() const { return grabbed_; }

    float time_cur() const { return time_cur_; }
    void set_time_cur(float time_s);

    Ren::Vec2f time_range() const;

    void ZoomIn();
    void ZoomOut();

    void Draw(Gui::Renderer *r) override;

    void Resize(const Gui::BaseElement *parent) override;
    using BaseElement::Resize;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Focus(const Ren::Vec2f &p) override;

    void PressRMB(const Ren::Vec2f &p, bool push);

    Sys::Signal<void(float, float, float)> time_changed_signal;
};
