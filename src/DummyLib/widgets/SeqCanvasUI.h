#pragma once

#include <Gui/BaseElement.h>
#include <Eng/utils/ScriptedSequence.h>

#include "TimelineUI.h"

class SeqCanvasUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;

    Gui::Image9Patch back_, time_cursor_, element_normal_, element_highlighted_;
    Gui::Image9Patch end_;
    float time_range_[2] = {0.0f, 8.0f};
    float time_cur_ = 1;

    Eng::ScriptedSequence *sequence_ = nullptr;

    Gui::Vec2i selected_index_ = Gui::Vec2i{-1};
    enum eDragFlags { DragBeg = (1u << 0u), DragEnd = (1u << 1u) };
    uint32_t selected_drag_flags_ = 0;
    Gui::Vec2f selected_pos_;
    float selected_time_beg_ = 0, selected_time_end_ = 0;

    float GetTimeFromPoint(float px);
    float GetPointFromTime(float t);
    Eng::SeqAction *GetActionAtPoint(const Gui::Vec2f &p, Gui::Vec2i &out_index, uint32_t &flags);
    Gui::Vec2f SnapToPixels(const Gui::Vec2f &p);

  public:
    SeqCanvasUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                const Gui::BaseElement *parent);

    void set_sequence(Eng::ScriptedSequence *seq) { sequence_ = seq; }

    void Draw(Gui::Renderer *r) override;

    void Resize() override;
    using BaseElement::Resize;

    //void Press(const Gui::Vec2f &p, bool push) override;
    //void Hover(const Gui::Vec2f &p) override;

    void OnCurTimeChange(float time_cur, float time_range_beg, float time_range_end);
};
