#include "TimelineUI.h"

#include <cmath>

#include <Gui/BitmapFont.h>

namespace TimelineUIConstants {
const float TimeScales[] = {0.5f, 0.625f, 0.75f, 0.875f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f};
const float TimeStepSmall = 0.1f;
} // namespace TimelineUIConstants

TimelineUI::TimelineUI(Ren::Context &ctx, const Gui::BitmapFont &font, const Gui::Vec2f &pos, const Gui::Vec2f &size,
                       const Gui::BaseElement *parent)
    : Gui::BaseElement{pos, size, parent}, font_(font),
      back_{ctx,
            "assets_pc/textures/editor/timeline_back.uncompressed.tga",
            Gui::Vec2f{1.0f, 1.5f},
            1.0f,
            Gui::Vec2f{-1},
            Gui::Vec2f{2},
            this},
      ruler_small_{ctx, "assets_pc/textures/editor/ruler_small.uncompressed.tga", Gui::Vec2f{}, Gui::Vec2f{}, parent},
      ruler_medium_{ctx, "assets_pc/textures/editor/ruler_medium.uncompressed.tga", Gui::Vec2f{}, Gui::Vec2f{}, parent},
      ruler_big_{ctx, "assets_pc/textures/editor/ruler_big.uncompressed.tga", Gui::Vec2f{}, Gui::Vec2f{}, parent},
      time_pos_{ctx, "assets_pc/textures/editor/time_pos.uncompressed.tga", Gui::Vec2f{}, Gui::Vec2f{}, parent},
      time_scale_index_(2), time_offset_(-0.5f), time_cur_(0.0f), time_step_(0.1f), grabbed_(false),
      snap_to_grid_(true), grabbed_rmb_(false) {}

void TimelineUI::set_time_cur(const float time_s) {
    time_cur_ = time_s;

    const Gui::Vec2f time_range_cur = time_range();
    time_changed_signal.FireN(time_cur_, time_range_cur[0], time_range_cur[1]);
}

Gui::Vec2f TimelineUI::time_range() const {
    const float unit_length = 256.0f / dims_px_[1][0];
    const float time_scale = TimelineUIConstants::TimeScales[time_scale_index_];

    const float time_range_beg = time_offset_;
    const float time_range_end = time_offset_ + 2.0f / (time_scale * unit_length);

    return Gui::Vec2f{time_range_beg, time_range_end};
}

void TimelineUI::ZoomIn() {
    const Gui::Vec2f time_range_before = time_range();
    const float time_mid_before = 0.5f * (time_range_before[0] + time_range_before[1]);

    time_scale_index_ = std::max(--time_scale_index_, 0);

    const Gui::Vec2f time_range_after = time_range();
    const float time_mid_after = 0.5f * (time_range_after[0] + time_range_after[1]);

    // adjust time offset to make center stable
    time_offset_ -= (time_mid_after - time_mid_before);

    // time range was updated
    set_time_cur(time_cur_);
}

void TimelineUI::ZoomOut() {
    const Gui::Vec2f time_range_before = time_range();
    const float time_mid_before = 0.5f * (time_range_before[0] + time_range_before[1]);

    time_scale_index_ = std::min(++time_scale_index_, int(sizeof(TimelineUIConstants::TimeScales) / sizeof(float) - 1));

    const Gui::Vec2f time_range_after = time_range();
    const float time_mid_after = 0.5f * (time_range_after[0] + time_range_after[1]);

    // adjust time offset to make center stable
    time_offset_ -= (time_mid_after - time_mid_before);

    // time range was updated
    set_time_cur(time_cur_);
}

void TimelineUI::Draw(Gui::Renderer *r) {
    using namespace TimelineUIConstants;

    back_.Draw(r);

    const float font_height = font_.height(this);
    const Gui::Vec2f time_range_cur = time_range();

    const float t_beg = TimeStepSmall * std::ceil(time_range_cur[0] / TimeStepSmall);
    const float t_end = TimeStepSmall * std::floor(time_range_cur[1] / TimeStepSmall);

    const float RoundingThres = 0.5f * TimeStepSmall;

    for (float t = t_beg; t < t_end + 0.5f * TimeStepSmall; t += TimeStepSmall) { // NOLINT
        // minus zero fixup
        if (std::abs(t) < RoundingThres) {
            t = 0;
        }

        const float px = GetPointFromTime(t);

        if (std::abs(t - std::round(t)) < RoundingThres) {
            // large tick
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f", t);

            const float width = font_.GetWidth(buf, this);
            font_.DrawText(r, buf,
                           SnapToPixels(Gui::Vec2f{px - 0.5f * width, 1.0f - 4.0f / dims_px_[1][1] - font_height}),
                           Gui::ColorWhite, this);

            ruler_big_.ResizeToContent(
                SnapToPixels(Gui::Vec2f{px - 8.0f / dims_px_[1][0], -1.0f + 2.0f / dims_px_[1][1]}));
            ruler_big_.Draw(r);
        } else if (std::abs(2.0f * t - std::round(2.0f * t)) < RoundingThres) {
            // medium tick
            ruler_medium_.ResizeToContent(
                SnapToPixels(Gui::Vec2f{px - 8.0f / dims_px_[1][0], -1.0f + 2.0f / dims_px_[1][1]}));
            ruler_medium_.Draw(r);
        } else if (std::abs(10.0f * t - std::round(10.0f * t)) < RoundingThres) {
            // small tick
            ruler_small_.ResizeToContent(
                SnapToPixels(Gui::Vec2f{px - 8.0f / dims_px_[1][0], -1.0f + 2.0f / dims_px_[1][1]}));
            ruler_small_.Draw(r);
        }
    }

    if (time_cur_ >= time_range_cur[0] && time_cur_ <= time_range_cur[1]) {
        // draw cursor
        const Gui::Vec2f time_pos = SnapToPixels(
            Gui::Vec2f{-1.0f + 2.0f * (time_cur_ - time_range_cur[0]) / (time_range_cur[1] - time_range_cur[0]) -
                           10.0f / dims_px_[1][0],
                       -1.0f});

        time_pos_.ResizeToContent(time_pos);
        time_pos_.Draw(r);
    }
}

void TimelineUI::Resize() {
    BaseElement::Resize();

    // force time range update
    set_time_cur(time_cur_);
}

/*void TimelineUI::Press(const Gui::Vec2f &p, const bool push) {
    if (push && Check(p)) {
        SetCurTimeFromPoint(p[0]);
        grabbed_ = true;
    } else {
        grabbed_ = false;
    }

    if (!push) {
        grabbed_ = false;
    }
}*/

#if 0
void TimelineUI::Hover(const Gui::Vec2f &p) {
    if (grabbed_ /*&& Check(p)*/) {
        SetCurTimeFromPoint(p[0]);
    } else if (grabbed_rmb_) {
        const Gui::Vec2f time_range_cur = time_range();

        const float dt = -(p[0] - rmb_point_[0]) / dims_[1][0];
        time_offset_ = rmb_time_offset_ + dt * (time_range_cur[1] - time_range_cur[0]);

        const Gui::Vec2f time_range_new = time_range();

        // time range was changed
        time_changed_signal.FireN(time_cur_, time_range_new[0], time_range_new[1]);
    }
}
#endif

void TimelineUI::PressRMB(const Gui::Vec2f &p, const bool push) {
    if (push && Check(p)) {
        rmb_point_ = p;
        rmb_time_offset_ = time_offset_;
        grabbed_rmb_ = true;
    } else {
        grabbed_rmb_ = false;
    }

    if (!push) {
        grabbed_rmb_ = false;
    }
}

float TimelineUI::GetTimeFromPoint(const float px) const {
    const Gui::Vec2f time_range_cur = time_range();

    const float param = (px - dims_[0][0]) / dims_[1][0];
    return time_range_cur[0] + param * (time_range_cur[1] - time_range_cur[0]);
}

float TimelineUI::GetPointFromTime(const float t) const {
    const Gui::Vec2f time_range_cur = time_range();

    const float param = (t - time_range_cur[0]) / (time_range_cur[1] - time_range_cur[0]);
    return -1.0f + param * 2.0f;
}

void TimelineUI::SetCurTimeFromPoint(const float px) {
    using namespace TimelineUIConstants;

    const Gui::Vec2f time_range_cur = time_range();

    const float t = std::min(std::max((px - dims_[0][0]) / dims_[1][0], 0.0f), 1.0f);
    time_cur_ = time_range_cur[0] + t * (time_range_cur[1] - time_range_cur[0]);

    if (snap_to_grid_) {
        time_cur_ = std::round(time_cur_ / TimeStepSmall) * TimeStepSmall;
    }

    time_cur_ = std::min(std::max(time_cur_, time_range_cur[0]), time_range_cur[1]);

    time_changed_signal.FireN(time_cur_, time_range_cur[0], time_range_cur[1]);
}
