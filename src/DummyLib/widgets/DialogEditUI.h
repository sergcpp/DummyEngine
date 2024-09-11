#pragma once

#include <functional>

#include <Gui/Image9Patch.h>
#include <Gui/Signal.h>

namespace Eng {
class ScriptedDialog;
class ScriptedSequence;
} // namespace Eng

class DialogEditUI : public Gui::BaseElement {
    const Gui::BitmapFont &font_;
    Gui::Image9Patch back_, element_, element_highlighted_;
    Gui::Image line_img_;
    Gui::Vec2f view_offset_ = Gui::Vec2f{-0.9f, 0.0f};

    bool grabbed_rmb_ = false;
    Gui::Vec2f rmb_point_;

    Eng::ScriptedDialog *dialog_ = nullptr;

    int selected_element_ = -1;
    uint64_t selected_timestamp_ = 0;

    void DrawLineLocal(Gui::Renderer *r, const Gui::Vec2f &p0, const Gui::Vec2f &p1, const Gui::Vec2f &width) const;
    void DrawCurveLocal(Gui::Renderer *r, const Gui::Vec2f &p0, const Gui::Vec2f &p1, const Gui::Vec2f &p2,
                        const Gui::Vec2f &p3, const Gui::Vec2f &width, const uint8_t color[4]) const;

    using IterationCallback = std::function<bool(const Eng::ScriptedSequence *seq, const Eng::ScriptedSequence *parent,
                                                 int depth, int ndx, int parent_ndx, int choice_ndx, bool visited)>;

    void IterateElements(const IterationCallback &callback);

  public:
    DialogEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                 Gui::BaseElement *parent);

    void set_dialog(Eng::ScriptedDialog *dialog) { dialog_ = dialog; }

    void Draw(Gui::Renderer *r) override;

    void Resize() override;

    //void Press(const Gui::Vec2f &p, bool push) override;
    //void Hover(const Gui::Vec2f &p) override;

    void PressRMB(const Gui::Vec2f &p, bool push);

    void OnSwitchSequence(int id);

    Gui::SignalN<void(int id)> set_cur_sequence_signal;
    Gui::SignalN<void(int id)> edit_cur_seq_signal;
};