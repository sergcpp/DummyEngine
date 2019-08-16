#pragma once

#include <functional>

#include <Eng/Gui/Image9Patch.h>
#include <Sys/Signal_.h>

class ScriptedDialog;
class ScriptedSequence;

class DialogEditUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;
    Gui::Image9Patch back_, element_, element_highlighted_;
    Gui::Image line_img_;
    Ren::Vec2f view_offset_ = Ren::Vec2f{-0.9f, 0.0f};

    bool grabbed_rmb_ = false;
    Ren::Vec2f rmb_point_;

    ScriptedDialog *dialog_ = nullptr;

    int selected_element_ = -1;
    uint64_t selected_timestamp_ = 0;

    Ren::Vec2f SnapToPixels(const Ren::Vec2f &p) const;
    void DrawLineLocal(Gui::Renderer *r, const Ren::Vec2f &p0, const Ren::Vec2f &p1, const Ren::Vec2f &width) const;
    void DrawCurveLocal(Gui::Renderer *r, const Ren::Vec2f &p0, const Ren::Vec2f &p1, const Ren::Vec2f &p2,
                        const Ren::Vec2f &p3, const Ren::Vec2f &width, const uint8_t color[4]) const;

    using IterationCallback = std::function<bool(const ScriptedSequence *seq, const ScriptedSequence *parent, int depth,
                                                 int ndx, int parent_ndx, int choice_ndx, bool visited)>;

    void IterateElements(const IterationCallback &callback);

  public:
    DialogEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Ren::Vec2f &pos, const Ren::Vec2f &size,
                 Gui::BaseElement *parent);

    void set_dialog(ScriptedDialog *dialog) { dialog_ = dialog; }

    void Draw(Gui::Renderer *r) override;

    void Resize(const Gui::BaseElement *parent) override;
    using BaseElement::Resize;

    void Press(const Ren::Vec2f &p, bool push) override;
    void Hover(const Ren::Vec2f &p) override;

    void PressRMB(const Ren::Vec2f &p, bool push);

    void OnSwitchSequence(int id);

    Sys::SignalN<void(int id)> set_cur_sequence_signal;
    Sys::SignalN<void(int id)> edit_cur_seq_signal;
};