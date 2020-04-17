#pragma once

#include <Eng/Gui/BaseElement.h>
#include <Eng/Utils/ScriptedSequence.h>

#include "TimelineUI.h"

class SeqCanvasUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;

    Gui::Image9Patch back_, time_cursor_, element_normal_, element_highlighted_;
    Gui::Image9Patch end_;
    float time_range_[2] = {0.0f, 8.0f};
    float time_cur_ = 1.0f;

    ScriptedSequence *sequence_ = nullptr;

    Ren::Vec2i highlighted_action_ = Ren::Vec2i{-1, -1};
    Ren::Vec2i selected_index_ = Ren::Vec2i{-1, -1};
    enum eDragFlags { DragBeg = (1u << 0u), DragEnd = (1u << 1u) };
    uint32_t selected_drag_flags_ = 0;
    Ren::Vec2f selected_pos_;
    float selected_time_beg_ = 0.0f, selected_time_end_ = 0.0f;

    float GetTimeFromPoint(float px);
    float GetPointFromTime(float t);
    SeqAction *GetActionAtPoint(const Ren::Vec2f &p, Ren::Vec2i &out_index,
                                uint32_t &flags);
    Ren::Vec2f SnapToPixels(const Ren::Vec2f &p);

  public:
    SeqCanvasUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos,
                const Gui::Vec2f &size, const Gui::BaseElement *parent);

    void set_sequence(ScriptedSequence *seq) { sequence_ = seq; }

    void Draw(Gui::Renderer *r) override;

    void Resize(const Gui::BaseElement *parent) override;
    using BaseElement::Resize;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Hover(const Ren::Vec2f &p) override;

    void OnCurTimeChange(float time_cur, float time_range_beg, float time_range_end);
};
