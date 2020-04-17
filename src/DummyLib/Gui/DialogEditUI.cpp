#include "DialogEditUI.h"

#include <Eng/Utils/ScriptedDialog.h>
#include <Eng/Utils/ScriptedSequence.h>

namespace DialogEditUIInternal {
const Ren::Vec2f ElementSizePx = Ren::Vec2f{96.0f, 128.0f};
const Ren::Vec2f ElementSpacingPx = Ren::Vec2f{300.0f, 160.0f};
} // namespace DialogEditUIInternal

DialogEditUI::DialogEditUI(Ren::Context &ctx, const Gui::BitmapFont &font,
                           const Ren::Vec2f &pos, const Ren::Vec2f &size,
                           Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), font_(font),
      back_(ctx, "assets_pc/textures/editor/dial_edit_back.uncompressed.tga",
            Ren::Vec2f{1.5f, 1.5f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f},
            Ren::Vec2f{2.0f, 2.0f}, this),
      element_(ctx, "assets_pc/textures/editor/square.uncompressed.tga",
               Ren::Vec2f{1.5f, 1.5f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f},
               Ren::Vec2f{2.0f, 2.0f}, this),
      element_highlighted_(
          ctx, "assets_pc/textures/editor/square_highlighted.uncompressed.tga",
          Ren::Vec2f{1.5f, 1.5f}, 1.0f, Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f},
          this),
      line_img_(ctx, "assets_pc/textures/editor/line.uncompressed.tga", Ren::Vec2f{},
                Ren::Vec2f{}, this) {}

void DialogEditUI::Draw(Gui::Renderer *r) {
    using namespace DialogEditUIInternal;

    back_.Draw(r);

    if (dialog_) {
        const Ren::Vec2f elem_size = 2.0f * ElementSizePx / Ren::Vec2f{size_px()};
        const Ren::Vec2f spacing = 2.0f * ElementSpacingPx / Ren::Vec2f{size_px()};

        const Ren::TextureRegionRef &line_tex = line_img_.tex();

        const Ren::Vec2f *_uvs = line_img_.uvs_px();
        const Ren::Vec2f uvs[2] = {_uvs[0] + Ren::Vec2f{1.0f, 0.0f},
                                   _uvs[1] - Ren::Vec2f{1.0f, 0.0f}};

        const Ren::Vec2f line_width =
            Ren::Vec2f{1.0f, aspect()} * 4.0f / Ren::Vec2f{size_px()};

        const Ren::Vec2f elem_border = 8.0f / Ren::Vec2f{size_px()};
        const float font_height = font_.height(this);

        r->PushClipArea(dims_);

        IterateElements([&](const ScriptedSequence *seq, const ScriptedSequence *parent,
                            const int depth, const int ndx, const int parent_ndx,
                            const int choice_ndx) {
            if (parent_ndx != -1) {
                // draw connection curve
                const Ren::Vec2f p0 =
                    view_offset_ +
                    Ren::Vec2f{spacing[0] * (depth - 1) + elem_size[0],
                               -spacing[1] * parent_ndx + 0.9f * elem_size[1] -
                                   0.1f * elem_size[1] * float(choice_ndx)};
                const Ren::Vec2f p3 =
                    view_offset_ + Ren::Vec2f{spacing[0] * depth,
                                              -spacing[1] * ndx + 0.5f * elem_size[1]};
                const Ren::Vec2f p1 = p0 + Ren::Vec2f{0.05f, 0.0f};
                const Ren::Vec2f p2 = p3 - Ren::Vec2f{0.05f, 0.0f};

                DrawCurveLocal(r, p0, p1, p2, p3, 10.0f * line_width);
                //DrawLineLocal(r, p1, p2, line_width);
            }

            const Ren::Vec2f elem_pos = SnapToPixels(
                view_offset_ + Ren::Vec2f{spacing[0] * depth, -spacing[1] * ndx});

            const int elem_index = int(seq - dialog_->GetSequence(0));
            Gui::Image9Patch *el =
                elem_index == selected_element_ ? &element_highlighted_ : &element_;
            const uint8_t *col =
                (elem_index == selected_element_) ? Gui::ColorCyan : Gui::ColorWhite;

            el->Resize(elem_pos, elem_size, this);
            el->Draw(r);

            { // draw info
                Ren::Vec2f text_pos =
                    elem_pos + Ren::Vec2f{elem_border[0],
                                          elem_size[1] - elem_border[1] - font_height};

                const char *seq_name = seq->name();
                if (seq_name) {
                    font_.DrawText(r, seq_name, text_pos, col, this);
                }
                text_pos[1] -= font_height;

                const double duration = seq->duration();

                char buf[16];
                sprintf(buf, "%.1fs", duration);

                font_.DrawText(r, buf, text_pos, col, this);
            }

            return true;
        });

        r->PopClipArea();
    }
}

void DialogEditUI::Resize(const Gui::BaseElement *parent) {
    BaseElement::Resize(parent);

    back_.Resize(this);
}

Ren::Vec2f DialogEditUI::SnapToPixels(const Ren::Vec2f &p) const {
    return Ren::Vec2f{
        std::round(0.5f * p[0] * dims_px_[1][0]) / (0.5f * float(dims_px_[1][0])),
        std::round(0.5f * p[1] * dims_px_[1][1]) / (0.5f * float(dims_px_[1][1]))};
}

void DialogEditUI::DrawLineLocal(Gui::Renderer *r, const Ren::Vec2f &_p0,
                                 const Ren::Vec2f &_p1,
                                 const Ren::Vec2f &line_width) const {
    const Ren::TextureRegionRef &line_tex = line_img_.tex();
    const Ren::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p0[1] + 1.0f) * dims_[1][1],
                               _uvs[0][0] + 2.0f, _uvs[0][1] + 0.5f};
    const auto p1 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p1[1] + 1.0f) * dims_[1][1],
                               _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f};

    r->DrawLine(Gui::eDrawMode::DrPassthrough, line_tex->pos(2), p0, p1,
                Ren::Vec4f{line_width[0], line_width[1], 2.0f, 0.0f});
}

void DialogEditUI::DrawCurveLocal(Gui::Renderer *r, const Ren::Vec2f &_p0,
                                  const Ren::Vec2f &_p1, const Ren::Vec2f &_p2,
                                  const Ren::Vec2f &_p3,
                                  const Ren::Vec2f &line_width) const {
    const Ren::TextureRegionRef &line_tex = line_img_.tex();
    const Ren::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p0[1] + 1.0f) * dims_[1][1],
                               _uvs[0][0] + 2.0f, _uvs[0][1] + 0.5f};
    const auto p1 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p1[1] + 1.0f) * dims_[1][1],
                               _uvs[0][0] + 2.0f, _uvs[0][1] + 0.0f};
    const auto p2 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p2[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p2[1] + 1.0f) * dims_[1][1],
                               _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f };
    const auto p3 = Ren::Vec4f{dims_[0][0] + 0.5f * (_p3[0] + 1.0f) * dims_[1][0],
                               dims_[0][1] + 0.5f * (_p3[1] + 1.0f) * dims_[1][1],
                               _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f };

    r->DrawCurve(Gui::eDrawMode::DrPassthrough, line_tex->pos(2), p0, p1, p2, p3,
        Ren::Vec4f{ line_width[0], line_width[1], 2.0f, 0.0f });
    //r->DrawLine(Gui::eDrawMode::DrPassthrough, line_tex->pos(2), p0, p1,
    //            Ren::Vec4f{line_width[0], line_width[1], 2.0f, 0.0f});
}

void DialogEditUI::IterateElements(
    std::function<bool(const ScriptedSequence *seq, const ScriptedSequence *parent,
                       int depth, int ndx, int parent_ndx, int choice_ndx)>
        callback) {
    struct entry_t {
        int id, parent_id;
        int depth;
        int ndx, parent_ndx, choice_ndx;
    } queue[128];
    int queue_size = 0;

    queue[queue_size++] = {0, 0, 0, 0, -1, -1};
    int levels[32] = {};

    while (queue_size--) {
        const entry_t e = queue[0];
        memmove(&queue[0], &queue[1], queue_size * sizeof(queue[0]));

        ScriptedSequence *seq = dialog_->GetSequence(e.id);
        assert(seq);
        ScriptedSequence *parent = dialog_->GetSequence(e.parent_id);
        assert(parent);

        if (!callback(seq, parent, e.depth, e.ndx, e.parent_ndx, e.choice_ndx)) {
            break;
        }

        const int choices_count = seq->GetChoicesCount();
        const int child_depth = e.depth + 1;
        for (int i = 0; i < choices_count; i++) {
            const SeqChoice *choice = seq->GetChoice(i);
            queue[queue_size++] = {choice->seq_id,      e.id,  child_depth,
                                   levels[child_depth], e.ndx, i};
            levels[child_depth]++;
        }
    }
}

void DialogEditUI::Press(const Ren::Vec2f &p, bool push) {
    using namespace DialogEditUIInternal;

    if (push && Check(p)) {
        const Ren::Vec2f elem_size = 2.0f * ElementSizePx / Ren::Vec2f{size_px()};
        const Ren::Vec2f spacing = 2.0f * ElementSpacingPx / Ren::Vec2f{size_px()};

        selected_element_ = -1;

        IterateElements([&](const ScriptedSequence *seq, const ScriptedSequence *parent,
                            const int depth, const int ndx, const int parent_ndx,
                            const int choice_ndx) -> bool {
            const Ren::Vec2f elem_pos = SnapToPixels(
                view_offset_ + Ren::Vec2f{spacing[0] * depth, -spacing[1] * ndx});

            element_.Resize(elem_pos, elem_size, this);

            if (element_.Check(p)) {
                selected_element_ = int(seq - dialog_->GetSequence(0));
                set_cur_sequence_signal.FireN(selected_element_);
                return false;
            }
            return true;
        });
    }
}

void DialogEditUI::Hover(const Ren::Vec2f &p) {
    if (grabbed_rmb_) {
        view_offset_ += 2.0f * (p - rmb_point_) / size();
        rmb_point_ = p;
    }
}

void DialogEditUI::PressRMB(const Ren::Vec2f &p, bool push) {
    if (push && Check(p)) {
        rmb_point_ = p;
        grabbed_rmb_ = true;
    } else {
        grabbed_rmb_ = false;
    }

    if (!push) {
        grabbed_rmb_ = false;
    }
}