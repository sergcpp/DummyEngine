#include "DialogEditUI.h"

#include <Eng/utils/ScriptedDialog.h>
#include <Eng/utils/ScriptedSequence.h>
#include <Gui/BitmapFont.h>
#include <Sys/Time_.h>

#pragma warning(push)
#pragma warning(disable : 6262) // Function uses a lot of stack

namespace DialogEditUIInternal {
const Gui::Vec2f ElementSizePx = Gui::Vec2f{96, 128};
const Gui::Vec2f ElementSpacingPx = Gui::Vec2f{300, 160};

const uint8_t ColorOrange[4] = {251, 126, 20, 255};
} // namespace DialogEditUIInternal

DialogEditUI::DialogEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos,
                           const Gui::Vec2f &size, Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), font_(font),
      back_(ctx, "assets_pc/textures/internal/back.dds", Gui::Vec2f{1.5f}, 1, Gui::Vec2f{-1}, Gui::Vec2f{2},
            this),
      element_(ctx, "assets_pc/textures/internal/square.dds", Gui::Vec2f{1.5f}, 1, Gui::Vec2f{-1},
               Gui::Vec2f{2}, this),
      element_highlighted_(ctx, "assets_pc/textures/internal/square_highlighted.dds", Gui::Vec2f{1.5f}, 1,
                           Gui::Vec2f{-1}, Gui::Vec2f{2}, this),
      line_img_(ctx, "assets_pc/textures/internal/line.dds", Gui::Vec2f{}, Gui::Vec2f{}, this) {}

void DialogEditUI::Draw(Gui::Renderer *r) {
    using namespace DialogEditUIInternal;

    back_.Draw(r);

    if (dialog_) {
        const Gui::Vec2f elem_size = 2 * ElementSizePx / Gui::Vec2f{size_px()};
        const Gui::Vec2f spacing = 2 * ElementSpacingPx / Gui::Vec2f{size_px()};

        const Ren::ImageRegionRef &line_tex = line_img_.tex();

        const Gui::Vec2f line_width = Gui::Vec2f{1, aspect()} * 4 / Gui::Vec2f{size_px()};

        const Gui::Vec2f elem_border = 8 / Gui::Vec2f{size_px()};
        const float font_height = font_.height(this);

        r->PushClipArea(dims_);

        IterateElements([&](const Eng::ScriptedSequence *seq, const Eng::ScriptedSequence *parent, const int depth,
                            const int ndx, const int parent_ndx, const int choice_ndx, const bool visited) {
            const int elem_index = int(seq - dialog_->GetSequence(0));

            if (parent_ndx != -1) {
                // draw connection curve
                const Gui::Vec2f p0 = view_offset_ + Gui::Vec2f{spacing[0] * float(depth - 1) + elem_size[0],
                                                                -spacing[1] * float(parent_ndx) + 0.75f * elem_size[1] -
                                                                    0.1f * elem_size[1] * float(choice_ndx)};
                const Gui::Vec2f p3 = view_offset_ + Gui::Vec2f{spacing[0] * float(depth),
                                                                -spacing[1] * float(ndx) + 0.5f * elem_size[1]};
                const Gui::Vec2f p1 = p0 + Gui::Vec2f{0.1f, 0.0f};
                const Gui::Vec2f p2 = p3 - Gui::Vec2f{0.1f, 0.0f};

                const uint8_t *curve_color = (elem_index == selected_element_) ? ColorOrange : Gui::ColorBlack;

                DrawCurveLocal(r, p0, p1, p2, p3, line_width, curve_color);

                // draw choice id
                const Eng::SeqChoice *choice = parent->GetChoice(choice_ndx);
                assert(choice);

                const uint8_t *text_color = (elem_index == selected_element_) ? Gui::ColorWhite : Gui::ColorBlack;

                const float width = font_.GetWidth(choice->key, this);

                font_.DrawText(r, choice->key, SnapToPixels(p0 + Gui::Vec2f{-width - elem_border[0], 0}), text_color,
                               this);
            }

            if (!visited) {
                const Gui::Vec2f elem_pos =
                    SnapToPixels(view_offset_ + Gui::Vec2f{spacing[0] * float(depth), -spacing[1] * float(ndx)});

                Gui::Image9Patch *el = elem_index == selected_element_ ? &element_highlighted_ : &element_;
                const uint8_t *col = (elem_index == selected_element_) ? Gui::ColorCyan : Gui::ColorWhite;

                el->Resize(elem_pos, elem_size);
                el->Draw(r);

                { // draw info
                    Gui::Vec2f text_pos =
                        elem_pos + Gui::Vec2f{elem_border[0], elem_size[1] - elem_border[1] - font_height};

                    std::string_view seq_name = seq->name();
                    if (!seq_name.empty()) {
                        font_.DrawText(r, seq_name, text_pos, col, this);
                    }
                    text_pos[1] -= font_height;

                    const double duration = seq->duration();

                    // TODO: refactor this
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%.1fs", duration);

                    font_.DrawText(r, buf, text_pos, col, this);
                }
            }

            return true;
        });

        r->PopClipArea();
    }
}

void DialogEditUI::Resize() { BaseElement::Resize(); }

void DialogEditUI::DrawLineLocal(Gui::Renderer *r, const Gui::Vec2f &_p0, const Gui::Vec2f &_p1,
                                 const Gui::Vec2f &width) const {
    const Ren::ImageRegionRef &line_tex = line_img_.tex();
    const Gui::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p0[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.5f};
    const auto p1 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p1[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};
    const auto dp = Normalize(Gui::Vec2f{p1 - p0});

    r->PushLine(Gui::eDrawMode::Passthrough, line_tex->pos(2), Gui::ColorBlack, p0, p1, dp, dp,
                Gui::Vec4f{width[0], width[1], 2, 0});
}

void DialogEditUI::DrawCurveLocal(Gui::Renderer *r, const Gui::Vec2f &_p0, const Gui::Vec2f &_p1, const Gui::Vec2f &_p2,
                                  const Gui::Vec2f &_p3, const Gui::Vec2f &width, const uint8_t color[4]) const {
    const Ren::ImageRegionRef &line_tex = line_img_.tex();
    const Gui::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p0[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.5f};
    const auto p1 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p1[1] + 1) * dims_[1][1], _uvs[0][0] + 2, _uvs[0][1] + 0.0f};
    const auto p2 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p2[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p2[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};
    const auto p3 =
        Gui::Vec4f{dims_[0][0] + 0.5f * (_p3[0] + 1) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p3[1] + 1) * dims_[1][1], _uvs[1][0] - 2, _uvs[1][1] - 0.5f};

    r->PushCurve(Gui::eDrawMode::Passthrough, line_tex->pos(2), color, p0, p1, p2, p3,
                 Gui::Vec4f{width[0], width[1], 2, 0});
}

void DialogEditUI::IterateElements(const IterationCallback &callback) {
    if (!dialog_ || dialog_->empty()) {
        return;
    }

    struct entry_t {
        int id, parent_id;
        int depth;
        int ndx, parent_ndx, choice_ndx;
        bool visited;
    } queue[256];
    int queue_head = 0, queue_tail = 0;

    queue[queue_tail++] = {0, 0, 0, 0, -1, -1, false};
    int seq_ids[256][16] = {};
    std::fill(&seq_ids[0][0], &seq_ids[0][0] + sizeof(seq_ids) / sizeof(int), -1);

    while (queue_head != queue_tail) {
        const entry_t e = queue[queue_head];
        queue_head = (queue_head + 1) % 256;

        const Eng::ScriptedSequence *seq = dialog_->GetSequence(e.id);
        assert(seq);
        const Eng::ScriptedSequence *parent = dialog_->GetSequence(e.parent_id);
        assert(parent);

        if (!callback(seq, parent, e.depth, e.ndx, e.parent_ndx, e.choice_ndx, e.visited)) {
            break;
        }

        const int choices_count = seq->GetChoicesCount();
        const int child_depth = e.depth + 1;
        for (int i = 0; i < choices_count && !e.visited; i++) {
            const Eng::SeqChoice *choice = seq->GetChoice(i);

            int level = 0;
            bool visited = false;
            while (seq_ids[child_depth][level] != -1) {
                if (seq_ids[child_depth][level] == choice->seq_id) {
                    visited = true;
                    break;
                }
                ++level;
            }

            queue[queue_tail] = {choice->seq_id, e.id, child_depth, level, e.ndx, i, visited};
            queue_tail = (queue_tail + 1) % 256;
            seq_ids[child_depth][level] = choice->seq_id;
        }
    }
}

/*void DialogEditUI::Press(const Gui::Vec2f &p, const bool push) {
    using namespace DialogEditUIInternal;

    if (push && Check(p)) {
        const Gui::Vec2f elem_size = 2 * ElementSizePx / Gui::Vec2f{size_px()};
        const Gui::Vec2f spacing = 2 * ElementSpacingPx / Gui::Vec2f{size_px()};

        selected_element_ = -1;

        IterateElements([&](const Eng::ScriptedSequence *seq, const Eng::ScriptedSequence *parent, const int depth,
                            const int ndx, const int parent_ndx, const int choice_ndx, const bool visited) -> bool {
            if (visited) {
                return true;
            }
            const Gui::Vec2f elem_pos =
                SnapToPixels(view_offset_ + Gui::Vec2f{spacing[0] * float(depth), -spacing[1] * float(ndx)});

            element_.Resize(elem_pos, elem_size);

            if (element_.Check(p)) {
                selected_element_ = int(seq - dialog_->GetSequence(0));
                set_cur_sequence_signal.FireN(selected_element_);
                if (Sys::GetTimeMs() - selected_timestamp_ < 300) {
                    edit_cur_seq_signal.FireN(selected_element_);
                }
                selected_timestamp_ = Sys::GetTimeMs();
                return false;
            }
            return true;
        });
    }
}*/

/*void DialogEditUI::Hover(const Gui::Vec2f &p) {
    if (grabbed_rmb_) {
        view_offset_ += 2 * (p - rmb_point_) / size();
        rmb_point_ = p;
    }
}*/

void DialogEditUI::PressRMB(const Gui::Vec2f &p, const bool push) {
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

void DialogEditUI::OnSwitchSequence(const int id) { selected_element_ = id; }

#pragma warning(pop)