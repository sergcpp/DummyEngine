#include "SeqCanvasUI.h"

#include <Gui/BitmapFont.h>
#include <Gui/Image.h>

namespace SeqCanvasUIInternal {
const int TrackCount = 8;
const int ElementCropRegionPx = 12;
const float ElementMoveStep = 0.1f;
const float ElementDurationMin = 0.3f;
} // namespace SeqCanvasUIInternal

SeqCanvasUI::SeqCanvasUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                         const Gui::BaseElement *parent)
    : Gui::BaseElement(pos, size, parent), font_(font), back_{ctx,
                                                              "assets_pc/textures/editor/canvas_back.uncompressed.tga",
                                                              Gui::Vec2f{1.0f, 1.5f},
                                                              1,
                                                              Gui::Vec2f{-1},
                                                              Gui::Vec2f{2},
                                                              this},
      time_cursor_{ctx,
                   "assets_pc/textures/editor/line_pointer.uncompressed.tga",
                   Gui::Vec2f{1.0f, 1.5f},
                   1.0f,
                   Gui::Vec2f{-1},
                   Gui::Vec2f{2},
                   this},
      element_normal_(ctx, "assets_pc/textures/editor/seq_el.uncompressed.tga", Gui::Vec2f{12.5f, 1.5f}, 1,
                      Gui::Vec2f{-1}, Gui::Vec2f{2}, this),
      element_highlighted_(ctx, "assets_pc/textures/editor/seq_el_highlighted.uncompressed.tga",
                           Gui::Vec2f{12.5f, 1.5f}, 1, Gui::Vec2f{-1}, Gui::Vec2f{2}, this),
      end_(ctx, "assets_pc/textures/editor/canvas_end.uncompressed.tga", Gui::Vec2f{1.0f, 1.5f}, 1, Gui::Vec2f{-1},
           Gui::Vec2f{2}, this) {
    SeqCanvasUI::Resize();
}

void SeqCanvasUI::Draw(Gui::Renderer *r) {
    using namespace SeqCanvasUIInternal;

    const float track_height = 2 / TrackCount;
    const float border_width = 4 / float(size_px()[0]);
    const float border_height = 4 / float(size_px()[1]);
    const float crop_region_width = float(2 * ElementCropRegionPx) / float(size_px()[0]);
    const float font_height = font_.height(this);

    for (int track = 0; track < TrackCount; track++) {
        const float y_coord = 1 - float(track + 1) * track_height;

        // draw background
        back_.Resize(Gui::Vec2f{-1, y_coord}, Gui::Vec2f{2, track_height});
        back_.Draw(r);

        // draw elements
        if (sequence_) {
            char str_buf[128];

            std::string_view name = sequence_->GetTrackName(track);
            std::string_view target = sequence_->GetTrackTarget(track);
            if (!name.empty() && !target.empty()) {
                snprintf(str_buf, sizeof(str_buf), "%s|%s", name.data(), target.data());
                font_.DrawText(r, str_buf, Gui::Vec2f{-1 + border_width, y_coord + font_height}, Gui::ColorBlack, this);
            }

            const int actions_count = sequence_->GetActionsCount(track);
            for (int action = 0; action < actions_count; action++) {
                const Eng::SeqAction *seq_action = sequence_->GetAction(track, action);

                const float x_beg = GetPointFromTime(float(seq_action->time_beg));
                const float x_end = GetPointFromTime(float(seq_action->time_end));

                // text clip area in relative coordinates
                Gui::Vec2f text_clip[2] = {
                    SnapToPixels(Gui::Vec2f{x_beg + crop_region_width, y_coord + border_height}),
                    Gui::Vec2f{x_end - x_beg - 2 * crop_region_width, track_height - 2 * border_height}};
                // convert to absolute coordinates
                text_clip[0] = pos() + 0.5f * (text_clip[0] + Gui::Vec2f(1, 1)) * size();
                text_clip[1] = 0.5f * text_clip[1] * size();

                r->PushClipArea(text_clip);

                Gui::Image9Patch *el = (track == selected_index_[0] && action == selected_index_[1])
                                           ? &element_highlighted_
                                           : &element_normal_;

                if (seq_action->sound_wave_tex) {
                    const Ren::TextureRegionRef &t = seq_action->sound_wave_tex;

                    const Ren::TexParams &p = t->params;
                    const Gui::Vec2f uvs_px[] = {Gui::Vec2f{float(t->pos(0)), float(t->pos(1))},
                                                 Gui::Vec2f{float(t->pos(0) + p.w), float(t->pos(1) + p.h)}};
                    const int tex_layer = t->pos(2);

                    const float x_beg_sound = GetPointFromTime(float(seq_action->time_beg + seq_action->sound_offset));
                    const float x_end_sound = GetPointFromTime(float(seq_action->time_beg + seq_action->sound_offset) +
                                                               Eng::SeqAction::SoundWaveStepS * float(p.w));
                    el->Resize(SnapToPixels(Gui::Vec2f{x_beg_sound, y_coord + border_height}),
                               Gui::Vec2f{x_end_sound - x_beg_sound, track_height - 2 * border_height});

                    const Gui::Vec2f pos[2] = {el->dims()[0], el->dims()[0] + el->dims()[1]};

                    r->PushImageQuad(Gui::eDrawMode::Passthrough, tex_layer, Gui::ColorWhite, pos, uvs_px);
                }

                const char *type_name = Eng::ScriptedSequence::ActionTypeNames[int(seq_action->type)];
                snprintf(str_buf, sizeof(str_buf), "[%s]", type_name);

                float y_text_pos = y_coord + track_height - 2 * border_height - font_height;

                font_.DrawText(r, str_buf, SnapToPixels(Gui::Vec2f{x_beg + 1.25f * crop_region_width, y_text_pos}),
                               Gui::ColorCyan, this);
                y_text_pos -= font_height;

                if (seq_action->anim_ref) {
                    font_.DrawText(r, seq_action->anim_ref->name(),
                                   SnapToPixels(Gui::Vec2f{x_beg + 1.25f * crop_region_width, y_text_pos}),
                                   Gui::ColorWhite, this);
                }

                r->PopClipArea();

                el->Resize(SnapToPixels(Gui::Vec2f{x_beg, y_coord + border_height}),
                           Gui::Vec2f{x_end - x_beg, track_height - 2 * border_height});
                el->Draw(r);
            }
        }
    }

    if (time_cur_ >= time_range_[0] && time_cur_ <= time_range_[1]) {
        // draw line pointer
        const Gui::Vec2f time_pos = SnapToPixels(Gui::Vec2f{
            -1 + 2 * (time_cur_ - time_range_[0]) / (time_range_[1] - time_range_[0]) - 10 / dims_px_[1][0], -1});

        time_cursor_.ResizeToContent(time_pos);

        const float width = 2 * float(time_cursor_.size_px()[0]) / float(size_px()[0]);
        time_cursor_.Resize(time_pos, Gui::Vec2f{width, 2});

        time_cursor_.Draw(r);
    }

    if (sequence_) {
        const auto end_time = float(sequence_->duration());
        if (end_time >= time_range_[0] && end_time <= time_range_[1]) {
            const float xpos = GetPointFromTime(end_time) - 10 / dims_px_[1][0];

            end_.Resize(Gui::Vec2f{xpos, -1}, Gui::Vec2f{20.0f / dims_px_[1][0], 2});
            end_.Draw(r);
        }

        std::string_view lookup_name = sequence_->lookup_name();
        if (!lookup_name.empty()) {
            const float width = font_.GetWidth(lookup_name, this);
            font_.DrawText(r, lookup_name, Gui::Vec2f{1 - width, -1 + font_height}, Gui::ColorWhite, this);
        }
    }
}

void SeqCanvasUI::Resize() { BaseElement::Resize(); }

/*void SeqCanvasUI::Press(const Gui::Vec2f &p, const bool push) {
    if (push) {
        const Eng::SeqAction *sel_action = GetActionAtPoint(p, selected_index_, selected_drag_flags_);
        if (sel_action) {
            selected_pos_ = p;
            selected_time_beg_ = float(sel_action->time_beg);
            selected_time_end_ = float(sel_action->time_end);
        }
    } else {
        selected_index_ = Gui::Vec2i{-1};
    }
}*/

/*void SeqCanvasUI::Hover(const Gui::Vec2f &p) {
    using namespace SeqCanvasUIInternal;

    if (selected_index_ != Gui::Vec2i{-1}) {
        Eng::SeqAction *action = sequence_->GetAction(selected_index_[0], selected_index_[1]);
        if (action) {
            const float time_prev = GetTimeFromPoint(selected_pos_[0]);
            const float time_next = GetTimeFromPoint(p[0]);
            const float time_delta = time_next - time_prev;

            const float time_delta_discrete = std::floor(time_delta / ElementMoveStep) * ElementMoveStep;

            if (selected_drag_flags_ & DragBeg) {
                action->time_beg = double(selected_time_beg_ + time_delta_discrete);
            }
            if (selected_drag_flags_ & DragEnd) {
                action->time_end = double(selected_time_end_ + time_delta_discrete);
            }

            const double duration = std::max(action->time_end - action->time_beg, (double)ElementDurationMin);
            action->time_end = action->time_beg + duration;

            sequence_->Reset();
        }
    }
}*/

void SeqCanvasUI::OnCurTimeChange(const float time_cur, const float time_range_beg, const float time_range_end) {
    time_cur_ = time_cur;
    time_range_[0] = time_range_beg;
    time_range_[1] = time_range_end;
}

float SeqCanvasUI::GetTimeFromPoint(const float px) {
    const float param = (px - dims_[0][0]) / dims_[1][0];
    return time_range_[0] + param * (time_range_[1] - time_range_[0]);
}

float SeqCanvasUI::GetPointFromTime(const float t) {
    const float param = (t - time_range_[0]) / (time_range_[1] - time_range_[0]);
    return -1 + param * 2;
}

Eng::SeqAction *SeqCanvasUI::GetActionAtPoint(const Gui::Vec2f &p, Gui::Vec2i &out_index, uint32_t &flags) {
    using namespace SeqCanvasUIInternal;

    // check elements
    if (sequence_ && Check(p)) {
        const float track_height = dims_[1][1] / TrackCount;
        const float border_height = 4.0f / float(size_px()[1]);

        for (int track = 0; track < TrackCount; track++) {
            const float y_coord = dims_[0][1] + dims_[1][1] - float(track + 1) * track_height;
            if (p[1] < y_coord || p[1] > y_coord + track_height) {
                continue;
            }

            const int actions_count = sequence_->GetActionsCount(track);
            for (int action = 0; action < actions_count; action++) {
                Eng::SeqAction *seq_action = sequence_->GetAction(track, action);

                const float x_beg = GetPointFromTime(float(seq_action->time_beg));
                const float x_end = GetPointFromTime(float(seq_action->time_end));

                if (p[0] > x_beg && p[0] < x_end && p[1] > y_coord + border_height &&
                    p[1] < y_coord + track_height - 2.0f * border_height) {
                    out_index[0] = track;
                    out_index[1] = action;

                    const float CropRegionWidth = float(2 * ElementCropRegionPx) / float(size_px()[0]);

                    flags = DragBeg | DragEnd;
                    if (p[0] < x_beg + CropRegionWidth) {
                        flags &= ~DragEnd;
                    }
                    if (p[0] > x_end - CropRegionWidth) {
                        flags &= ~DragBeg;
                    }

                    return seq_action;
                }
            }
        }
    }
    out_index = Gui::Vec2i{-1};
    return nullptr;
}

Gui::Vec2f SeqCanvasUI::SnapToPixels(const Gui::Vec2f &p) {
    return Gui::Vec2f{std::round(0.5f * p[0] * dims_px_[1][0]) / (0.5f * float(dims_px_[1][0])),
                      std::round(0.5f * p[1] * dims_px_[1][1]) / (0.5f * float(dims_px_[1][1]))};
}