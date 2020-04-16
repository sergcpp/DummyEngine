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

        const Ren::Vec2f line_width = size() * 2.0f / Ren::Vec2f{size_px()};

        const Ren::Vec2f elem_border = 8.0f / Ren::Vec2f{size_px()};
        const float font_height = font_.height(this);

        r->PushClipArea(dims_);

        IterateElements([&](const ScriptedSequence *seq, const ScriptedSequence *parent,
                            const int depth, const int ndx, const int parent_ndx) {
            if (parent_ndx != -1) {
                // draw connection line
                const Ren::Vec2f line[2] = {
                    view_offset_ +
                        Ren::Vec2f{spacing[0] * (depth - 1) + elem_size[0],
                                   -spacing[1] * parent_ndx + 0.5f * elem_size[1]},
                    view_offset_ + Ren::Vec2f{spacing[0] * depth,
                                              -spacing[1] * ndx + 0.5f * elem_size[1]}};
                DrawLineLocal(r, line, line_width);
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

void DialogEditUI::DrawLineLocal(Gui::Renderer *r, const Ren::Vec2f line[2],
                                 const Ren::Vec2f &line_width) const {
    Ren::Vec2f _line[2];
    _line[0] = SnapToPixels(pos() + 0.5f * (line[0] + Ren::Vec2f(1, 1)) * size());
    _line[1] = SnapToPixels(pos() + 0.5f * (line[1] + Ren::Vec2f(1, 1)) * size());

    const Ren::TextureRegionRef &line_tex = line_img_.tex();

    const Ren::Vec2f *_uvs = line_img_.uvs_px();
    const Ren::Vec2f uvs[2] = {_uvs[0] + Ren::Vec2f{1.0f, 0.0f},
                               _uvs[1] - Ren::Vec2f{1.0f, 0.0f}};

    r->DrawLine(Gui::eDrawMode::DrPassthrough, line_tex->pos(2), _line, line_width, uvs);
}

void DialogEditUI::IterateElements(
    std::function<bool(const ScriptedSequence *seq, const ScriptedSequence *parent,
                       int depth, int ndx, int parent_ndx)>
        callback) {
    struct entry_t {
        int id, parent_id;
        int depth;
        int ndx, parent_ndx;
    } queue[128];
    int queue_size = 0;

    queue[queue_size++] = {0, 0, 0, 0, -1};
    int levels[32] = {};

    while (queue_size--) {
        const entry_t e = queue[0];
        memmove(&queue[0], &queue[1], queue_size * sizeof(queue[0]));

        ScriptedSequence *seq = dialog_->GetSequence(e.id);
        assert(seq);
        ScriptedSequence *parent = dialog_->GetSequence(e.parent_id);
        assert(parent);

        if (!callback(seq, parent, e.depth, e.ndx, e.parent_ndx)) {
            break;
        }

        const int choices_count = seq->GetChoicesCount();
        const int child_depth = e.depth + 1;
        for (int i = 0; i < choices_count; i++) {
            const SeqChoice *choice = seq->GetChoice(i);
            queue[queue_size++] = {choice->seq_id, e.id, child_depth, levels[child_depth],
                                   e.ndx};
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
                            const int depth, const int ndx,
                            const int parent_ndx) -> bool {
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

void DialogEditUI::Focus(const Ren::Vec2f &p) {
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