#include "DialogEditUI.h"

#include <Eng/gui/BitmapFont.h>
#include <Eng/utils/ScriptedDialog.h>
#include <Eng/utils/ScriptedSequence.h>
#include <Sys/Time_.h>

namespace DialogEditUIInternal {
const Ren::Vec2f ElementSizePx = Ren::Vec2f{96.0f, 128.0f};
const Ren::Vec2f ElementSpacingPx = Ren::Vec2f{300.0f, 160.0f};

const uint8_t ColorOrange[4] = {251, 126, 20, 255};
} // namespace DialogEditUIInternal

DialogEditUI::DialogEditUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Ren::Vec2f &pos,
                           const Ren::Vec2f &size, Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), font_(font),
      back_(ctx, "assets_pc/textures/editor/dial_edit_back.uncompressed.tga", Ren::Vec2f{1.5f, 1.5f}, 1.0f,
            Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f}, this),
      element_(ctx, "assets_pc/textures/editor/square.uncompressed.tga", Ren::Vec2f{1.5f, 1.5f}, 1.0f,
               Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f}, this),
      element_highlighted_(ctx, "assets_pc/textures/editor/square_highlighted.uncompressed.tga", Ren::Vec2f{1.5f, 1.5f},
                           1.0f, Ren::Vec2f{-1.0f, -1.0f}, Ren::Vec2f{2.0f, 2.0f}, this),
      line_img_(ctx, "assets_pc/textures/editor/line.uncompressed.tga", Ren::Vec2f{}, Ren::Vec2f{}, this) {}

void DialogEditUI::Draw(Gui::Renderer *r) {
    using namespace DialogEditUIInternal;

    back_.Draw(r);

    if (dialog_) {
        const Ren::Vec2f elem_size = 2.0f * ElementSizePx / Ren::Vec2f{size_px()};
        const Ren::Vec2f spacing = 2.0f * ElementSpacingPx / Ren::Vec2f{size_px()};

        const Ren::TextureRegionRef &line_tex = line_img_.tex();

        const Ren::Vec2f line_width = Ren::Vec2f{1.0f, aspect()} * 4.0f / Ren::Vec2f{size_px()};

        const Ren::Vec2f elem_border = 8.0f / Ren::Vec2f{size_px()};
        const float font_height = font_.height(this);

        r->PushClipArea(dims_);

        IterateElements([&](const Eng::ScriptedSequence *seq, const Eng::ScriptedSequence *parent, const int depth,
                            const int ndx, const int parent_ndx, const int choice_ndx, const bool visited) {
            const int elem_index = int(seq - dialog_->GetSequence(0));

            if (parent_ndx != -1) {
                // draw connection curve
                const Ren::Vec2f p0 = view_offset_ + Ren::Vec2f{spacing[0] * float(depth - 1) + elem_size[0],
                                                                -spacing[1] * float(parent_ndx) + 0.75f * elem_size[1] -
                                                                    0.1f * elem_size[1] * float(choice_ndx)};
                const Ren::Vec2f p3 = view_offset_ + Ren::Vec2f{spacing[0] * float(depth),
                                                                -spacing[1] * float(ndx) + 0.5f * elem_size[1]};
                const Ren::Vec2f p1 = p0 + Ren::Vec2f{0.1f, 0.0f};
                const Ren::Vec2f p2 = p3 - Ren::Vec2f{0.1f, 0.0f};

                const uint8_t *curve_color = (elem_index == selected_element_) ? ColorOrange : Gui::ColorBlack;

                DrawCurveLocal(r, p0, p1, p2, p3, line_width, curve_color);

                // draw choice id
                const Eng::SeqChoice *choice = parent->GetChoice(choice_ndx);
                assert(choice);

                const uint8_t *text_color = (elem_index == selected_element_) ? Gui::ColorWhite : Gui::ColorBlack;

                const float width = font_.GetWidth(choice->key, this);

                font_.DrawText(r, choice->key, SnapToPixels(p0 + Ren::Vec2f{-width - elem_border[0], 0.0f}), text_color,
                               this);
            }

            if (!visited) {
                const Ren::Vec2f elem_pos =
                    SnapToPixels(view_offset_ + Ren::Vec2f{spacing[0] * float(depth), -spacing[1] * float(ndx)});

                Gui::Image9Patch *el = elem_index == selected_element_ ? &element_highlighted_ : &element_;
                const uint8_t *col = (elem_index == selected_element_) ? Gui::ColorCyan : Gui::ColorWhite;

                el->Resize(elem_pos, elem_size, this);
                el->Draw(r);

                { // draw info
                    Ren::Vec2f text_pos =
                        elem_pos + Ren::Vec2f{elem_border[0], elem_size[1] - elem_border[1] - font_height};

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

void DialogEditUI::Resize(const Gui::BaseElement *parent) {
    BaseElement::Resize(parent);

    back_.Resize(this);
}

Ren::Vec2f DialogEditUI::SnapToPixels(const Ren::Vec2f &p) const {
    return Ren::Vec2f{std::round(0.5f * p[0] * dims_px_[1][0]) / (0.5f * float(dims_px_[1][0])),
                      std::round(0.5f * p[1] * dims_px_[1][1]) / (0.5f * float(dims_px_[1][1]))};
}

void DialogEditUI::DrawLineLocal(Gui::Renderer *r, const Ren::Vec2f &_p0, const Ren::Vec2f &_p1,
                                 const Ren::Vec2f &width) const {
    const Ren::TextureRegionRef &line_tex = line_img_.tex();
    const Ren::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p0[1] + 1.0f) * dims_[1][1], _uvs[0][0] + 2.0f, _uvs[0][1] + 0.5f};
    const auto p1 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p1[1] + 1.0f) * dims_[1][1], _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f};
    const auto dp = Normalize(Ren::Vec2f{p1 - p0});

    r->PushLine(Gui::eDrawMode::Passthrough, line_tex->pos(2), Gui::ColorBlack, p0, p1, dp, dp,
                Ren::Vec4f{width[0], width[1], 2.0f, 0.0f});
}

void DialogEditUI::DrawCurveLocal(Gui::Renderer *r, const Ren::Vec2f &_p0, const Ren::Vec2f &_p1, const Ren::Vec2f &_p2,
                                  const Ren::Vec2f &_p3, const Ren::Vec2f &width, const uint8_t color[4]) const {
    const Ren::TextureRegionRef &line_tex = line_img_.tex();
    const Ren::Vec2f *_uvs = line_img_.uvs_px();

    const auto p0 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p0[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p0[1] + 1.0f) * dims_[1][1], _uvs[0][0] + 2.0f, _uvs[0][1] + 0.5f};
    const auto p1 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p1[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p1[1] + 1.0f) * dims_[1][1], _uvs[0][0] + 2.0f, _uvs[0][1] + 0.0f};
    const auto p2 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p2[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p2[1] + 1.0f) * dims_[1][1], _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f};
    const auto p3 =
        Ren::Vec4f{dims_[0][0] + 0.5f * (_p3[0] + 1.0f) * dims_[1][0],
                   dims_[0][1] + 0.5f * (_p3[1] + 1.0f) * dims_[1][1], _uvs[1][0] - 2.0f, _uvs[1][1] - 0.5f};

    r->PushCurve(Gui::eDrawMode::Passthrough, line_tex->pos(2), color, p0, p1, p2, p3,
                 Ren::Vec4f{width[0], width[1], 2.0f, 0.0f});
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

void DialogEditUI::Press(const Ren::Vec2f &p, const bool push) {
    using namespace DialogEditUIInternal;

    if (push && Check(p)) {
        const Ren::Vec2f elem_size = 2.0f * ElementSizePx / Ren::Vec2f{size_px()};
        const Ren::Vec2f spacing = 2.0f * ElementSpacingPx / Ren::Vec2f{size_px()};

        selected_element_ = -1;

        IterateElements([&](const Eng::ScriptedSequence *seq, const Eng::ScriptedSequence *parent, const int depth,
                            const int ndx, const int parent_ndx, const int choice_ndx, const bool visited) -> bool {
            if (visited) {
                return true;
            }
            const Ren::Vec2f elem_pos =
                SnapToPixels(view_offset_ + Ren::Vec2f{spacing[0] * float(depth), -spacing[1] * float(ndx)});

            element_.Resize(elem_pos, elem_size, this);

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
}

void DialogEditUI::Hover(const Ren::Vec2f &p) {
    if (grabbed_rmb_) {
        view_offset_ += 2.0f * (p - rmb_point_) / size();
        rmb_point_ = p;
    }
}

void DialogEditUI::PressRMB(const Ren::Vec2f &p, const bool push) {
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