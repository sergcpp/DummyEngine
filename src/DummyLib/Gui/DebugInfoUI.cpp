#include "DebugInfoUI.h"

#include <Eng/gui/BitmapFont.h>
#include <Eng/renderer/Renderer_Structs.h>
#include <Eng/scene/SceneData.h>
#include <Sys/Time_.h>

DebugInfoUI::DebugInfoUI(const Ren::Vec2f &pos, const Ren::Vec2f &size, const BaseElement *parent,
                         std::shared_ptr<Gui::BitmapFont> font)
    : BaseElement(pos, size, parent), parent_(parent), font_(std::move(font)) {}

void DebugInfoUI::UpdateInfo(const Eng::FrontendInfo &frontend_info, const Eng::BackendInfo &backend_info,
                             const Eng::ItemsInfo &items_info, uint64_t render_flags) {
    const float alpha = 0.98f;
    const float k = (1.0f - alpha);

    auto us_to_ms = [](uint64_t v) -> float { return 0.001f * v; };
    auto ns_to_ms = [](uint64_t v) -> float { return float(0.000001 * double(v)); };

    render_flags_ = render_flags;

    front_info_smooth_.occluders_time_ms *= alpha;
    front_info_smooth_.occluders_time_ms += k * us_to_ms(frontend_info.occluders_time_us);
    front_info_smooth_.main_gather_time_ms *= alpha;
    front_info_smooth_.main_gather_time_ms += k * us_to_ms(frontend_info.main_gather_time_us);
    front_info_smooth_.shadow_gather_time_ms *= alpha;
    front_info_smooth_.shadow_gather_time_ms += k * us_to_ms(frontend_info.shadow_gather_time_us);
    front_info_smooth_.drawables_sort_time_ms *= alpha;
    front_info_smooth_.drawables_sort_time_ms += k * us_to_ms(frontend_info.drawables_sort_time_us);
    front_info_smooth_.items_assignment_time_ms *= alpha;
    front_info_smooth_.items_assignment_time_ms += k * us_to_ms(frontend_info.items_assignment_time_us);
    front_info_smooth_.total_time_ms *= alpha;
    front_info_smooth_.total_time_ms += k * us_to_ms(frontend_info.end_timepoint_us - frontend_info.start_timepoint_us);

    back_info_smooth_.pass_timings_count = int(backend_info.pass_timings.size());
    for (int i = 0; i < int(backend_info.pass_timings.size()); ++i) {
        if (back_info_smooth_.pass_names[i] == backend_info.pass_timings[i].name) {
            back_info_smooth_.pass_timings_ms[i] *= alpha;
            back_info_smooth_.pass_timings_ms[i] += k * ns_to_ms(backend_info.pass_timings[i].duration);
        } else {
            back_info_smooth_.pass_names[i] = backend_info.pass_timings[i].name;
            back_info_smooth_.pass_timings_ms[i] = ns_to_ms(backend_info.pass_timings[i].duration);
        }
    }

    back_info_smooth_.cpu_total_ms *= alpha;
    back_info_smooth_.cpu_total_ms +=
        k * us_to_ms(backend_info.cpu_end_timepoint_us - backend_info.cpu_start_timepoint_us);

    back_info_smooth_.gpu_total_ms *= alpha;
    back_info_smooth_.gpu_total_ms += k * ns_to_ms(backend_info.gpu_total_duration);

    items_info_smooth_.lights_count *= alpha;
    items_info_smooth_.lights_count += k * items_info.lights_count;
    items_info_smooth_.decals_count *= alpha;
    items_info_smooth_.decals_count += k * items_info.decals_count;
    items_info_smooth_.probes_count *= alpha;
    items_info_smooth_.probes_count += k * items_info.probes_count;
    items_info_smooth_.items_total *= alpha;
    items_info_smooth_.items_total += k * items_info.items_total;

    prev_timing_info_ = cur_timing_info_;
    cur_timing_info_.front_start_timepoint_us = frontend_info.start_timepoint_us;
    cur_timing_info_.front_end_timepoint_us = frontend_info.end_timepoint_us;
    cur_timing_info_.back_cpu_start_timepoint_us = backend_info.cpu_start_timepoint_us;
    cur_timing_info_.back_cpu_end_timepoint_us = backend_info.cpu_end_timepoint_us;
    cur_timing_info_.back_gpu_duration = backend_info.gpu_total_duration;
    cur_timing_info_.gpu_cpu_time_diff_us = backend_info.gpu_cpu_time_diff_us;
}

void DebugInfoUI::Draw(Gui::Renderer *r) {
    const float font_height = font_->height(parent_);

    const char delimiter[] = "-------------------------------";
    char text_buffer[256];

    float vertical_offset = 0.75f;
    static const uint8_t text_color[4] = {255, 255, 255, 100};

    { // fps counter
        const uint64_t cur_frame_time = Sys::GetTimeUs();

        const double last_frame_dur = double(cur_frame_time - last_frame_time_) * 0.001;

        last_frame_time_ = cur_frame_time;

        const double alpha = 0.025;
        cur_frame_dur_ = alpha * last_frame_dur + (1.0 - alpha) * cur_frame_dur_;

        sprintf(text_buffer, "              FPS: %.1f", (1000.0 / cur_frame_dur_));
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    { // renderer frontend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "   OCCLUDERS RAST: %.3f ms", front_info_smooth_.occluders_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "      MAIN GATHER: %.3f ms", front_info_smooth_.main_gather_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "    SHADOW GATHER: %.3f ms", front_info_smooth_.shadow_gather_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "          SORTING: %.3f ms", front_info_smooth_.drawables_sort_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "      ITEM ASSIGN: %.3f ms", front_info_smooth_.items_assignment_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "      FRONT TOTAL: %.3f ms", front_info_smooth_.total_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    { // renderer backend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        /*vertical_offset -= font_height;
        sprintf(text_buffer, "           DRAW CALLS: [%.1f, %.1f, %.1f]", back_info_smooth_.shadow_draw_calls_count,
                back_info_smooth_.depth_fill_draw_calls_count, back_info_smooth_.opaque_draw_calls_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "            TRIANGLES: %.2f M", back_info_smooth_.tris_rendered);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);*/

        for (int i = 0; i < back_info_smooth_.pass_timings_count; ++i) {
            vertical_offset -= font_height;
            sprintf(text_buffer, " %16s: %.3f ms", back_info_smooth_.pass_names[i].c_str(),
                    back_info_smooth_.pass_timings_ms[i]);
            font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
        }

        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "   BACK CPU TOTAL: %.3f ms", back_info_smooth_.cpu_total_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "   BACK GPU TOTAL: %.3f ms", back_info_smooth_.gpu_total_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    if (render_flags_ & (Eng::DebugLights | Eng::DebugDecals)) {
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "     LIGHTS COUNT: %.3f", items_info_smooth_.lights_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "      LIGHTS DATA: %.3f kb",
                items_info_smooth_.lights_count * sizeof(Eng::LightItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "     DECALS COUNT: %.3f", items_info_smooth_.decals_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "      DECALS DATA: %.3f kb",
                items_info_smooth_.decals_count * sizeof(Eng::DecalItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "       CELLS DATA: %.3f kb", REN_CELLS_COUNT * sizeof(Eng::CellData) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "       ITEMS DATA: %.3f kb",
                items_info_smooth_.items_total * sizeof(Eng::ItemData) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    if (render_flags_ & Eng::DebugTimings) {
        /*if (prev_timing_info_.front_end_timepoint_us) {
            auto prev_front_start = double(prev_timing_info_.front_start_timepoint_us),
                 prev_front_end = double(prev_timing_info_.front_end_timepoint_us),
                 prev_back_cpu_start = double(prev_timing_info_.back_cpu_start_timepoint_us),
                 prev_back_cpu_end = double(prev_timing_info_.back_cpu_end_timepoint_us),
                 prev_back_gpu_duration = double(prev_timing_info_.back_gpu_duration),
                 prev_swap_start = double(prev_timing_info_.swap_interval.start_timepoint_us),
                 prev_swap_end = double(prev_timing_info_.swap_interval.end_timepoint_us),
                 next_front_start = double(cur_timing_info_.front_start_timepoint_us),
                 next_front_end = double(cur_timing_info_.front_end_timepoint_us),
                 next_back_cpu_start = double(cur_timing_info_.back_cpu_start_timepoint_us),
                 next_back_cpu_end = double(cur_timing_info_.back_cpu_end_timepoint_us),
                 next_back_gpu_duration = double(cur_timing_info_.back_gpu_duration),
                 next_swap_start = double(cur_timing_info_.swap_interval.start_timepoint_us),
                 next_swap_end = double(cur_timing_info_.swap_interval.end_timepoint_us);

            prev_back_gpu_start -= double(prev_timing_info_.gpu_cpu_time_diff_us);
            prev_back_gpu_duration -= double(prev_timing_info_.gpu_cpu_time_diff_us);
            next_back_gpu_start -= double(cur_timing_info_.gpu_cpu_time_diff_us);
            next_back_gpu_end -= double(cur_timing_info_.gpu_cpu_time_diff_us);

            double start_point = prev_back_cpu_start;

            prev_front_start -= start_point;
            prev_front_end -= start_point;
            prev_back_cpu_start -= start_point;
            prev_back_cpu_end -= start_point;
            prev_back_gpu_start -= start_point;
            prev_back_gpu_end -= start_point;
            prev_swap_start -= start_point;
            prev_swap_end -= start_point;
            next_front_start -= start_point;
            next_front_end -= start_point;
            next_back_cpu_start -= start_point;
            next_back_cpu_end -= start_point;
            next_back_gpu_start -= start_point;
            next_back_gpu_end -= start_point;
            next_swap_start -= start_point;
            next_swap_end -= start_point;

            double dur = 0.0;
            int cc = 0;

            while (dur < std::max(next_front_end, next_back_gpu_end)) {
                dur += 1000000.0 / 60.0;
                cc++;
            }

            prev_front_start /= dur;
            prev_front_end /= dur;
            prev_back_cpu_start /= dur;
            prev_back_cpu_end /= dur;
            prev_back_gpu_start /= dur;
            prev_back_gpu_end /= dur;
            prev_swap_start /= dur;
            prev_swap_end /= dur;

            next_front_start /= dur;
            next_front_end /= dur;
            next_back_cpu_start /= dur;
            next_back_cpu_end /= dur;
            next_back_gpu_start /= dur;
            next_back_gpu_end /= dur;
            next_swap_start /= dur;
            next_swap_end /= dur;

            text_buffer[0] = '[';
            text_buffer[101] = ']';

            for (int i = 0; i < 100; i++) {
                const double t = double(i) / 100;

                if ((t >= prev_front_start && t <= prev_front_end) || (t >= next_front_start && t <= next_front_end)) {
                    text_buffer[i + 1] = 'F';
                } else {
                    text_buffer[i + 1] = '_';
                }
            }

            sprintf(&text_buffer[102], " [2 frames, %.1f ms]", cc * 1000.0 / 60.0);

            vertical_offset -= font_height;
            font_->DrawText(r, delimiter, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

            vertical_offset -= font_height;
            font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

            for (int i = 0; i < 100; i++) {
                const double t = double(i) / 100;

                if ((t >= prev_back_cpu_start && t <= prev_back_cpu_end) ||
                    (t >= next_back_cpu_start && t <= next_back_cpu_end)) {
                    text_buffer[i + 1] = 'B';
                } else if ((t >= prev_swap_start && t <= prev_swap_end) ||
                           (t >= next_swap_start && t <= next_swap_end)) {
                    text_buffer[i + 1] = 'S';
                } else {
                    text_buffer[i + 1] = '_';
                }
            }

            vertical_offset -= font_height;
            font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

            for (int i = 0; i < 100; i++) {
                const double t = double(i) / 100;

                if ((t >= prev_back_gpu_start && t <= prev_back_gpu_end) ||
                    (t >= next_back_gpu_start && t <= next_back_gpu_end)) {
                    text_buffer[i + 1] = 'G';
                } else {
                    text_buffer[i + 1] = '_';
                }
            }

            vertical_offset -= font_height;
            font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
        }*/
    }
}
