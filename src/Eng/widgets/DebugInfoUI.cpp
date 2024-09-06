#include "DebugInfoUI.h"

#include <Gui/BitmapFont.h>
#include <Sys/Time_.h>

#include "../renderer/Renderer_Structs.h"
#include "../scene/SceneData.h"

Eng::DebugInfoUI::DebugInfoUI(const Gui::Vec2f &pos, const Gui::Vec2f &size, const BaseElement *parent,
                              const Gui::BitmapFont *font)
    : BaseElement(pos, size, parent), parent_(parent), font_(font) {}

void Eng::DebugInfoUI::UpdateInfo(const Eng::FrontendInfo &frontend_info, const Eng::BackendInfo &backend_info,
                                  const Eng::ItemsInfo &items_info, const bool debug_items) {
    const float alpha = 0.98f;
    const float k = (1.0f - alpha);

    auto us_to_ms = [](uint64_t v) -> float { return 0.001f * v; };

    debug_items_ = debug_items;

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
            back_info_smooth_.pass_timings_ms[i] += k * us_to_ms(backend_info.pass_timings[i].duration);
        } else {
            back_info_smooth_.pass_names[i] = backend_info.pass_timings[i].name;
            back_info_smooth_.pass_timings_ms[i] = us_to_ms(backend_info.pass_timings[i].duration);
        }
    }

    back_info_smooth_.cpu_total_ms *= alpha;
    back_info_smooth_.cpu_total_ms +=
        k * us_to_ms(backend_info.cpu_end_timepoint_us - backend_info.cpu_start_timepoint_us);

    back_info_smooth_.gpu_total_ms *= alpha;
    back_info_smooth_.gpu_total_ms += k * us_to_ms(backend_info.gpu_total_duration);

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

void Eng::DebugInfoUI::Draw(Gui::Renderer *r) {
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

        snprintf(text_buffer, sizeof(text_buffer), "              FPS: %.1f", (1000.0 / cur_frame_dur_));
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    { // renderer frontend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "   OCCLUDERS RAST: %.3f ms", front_info_smooth_.occluders_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      MAIN GATHER: %.3f ms",
                 front_info_smooth_.main_gather_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "    SHADOW GATHER: %.3f ms",
                 front_info_smooth_.shadow_gather_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "          SORTING: %.3f ms",
                 front_info_smooth_.drawables_sort_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      ITEM ASSIGN: %.3f ms",
                 front_info_smooth_.items_assignment_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      FRONT TOTAL: %.3f ms", front_info_smooth_.total_time_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    { // renderer backend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        /*vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "           DRAW CALLS: [%.1f, %.1f, %.1f]",
        back_info_smooth_.shadow_draw_calls_count, back_info_smooth_.depth_fill_draw_calls_count,
        back_info_smooth_.opaque_draw_calls_count); font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset},
        text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "            TRIANGLES: %.2f M", back_info_smooth_.tris_rendered);
        font_->DrawText(r, text_buffer, Ren::Vec2f{-1.0f, vertical_offset}, text_color, parent_);*/

        for (int i = 0; i < back_info_smooth_.pass_timings_count; ++i) {
            vertical_offset -= font_height;
            snprintf(text_buffer, sizeof(text_buffer), " %16s: %.3f ms", back_info_smooth_.pass_names[i].c_str(),
                     back_info_smooth_.pass_timings_ms[i]);
            font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
        }

        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "   BACK CPU TOTAL: %.3f ms", back_info_smooth_.cpu_total_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "   BACK GPU TOTAL: %.3f ms", back_info_smooth_.gpu_total_ms);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }

    if (debug_items_) {
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "     LIGHTS COUNT: %.3f", items_info_smooth_.lights_count);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      LIGHTS DATA: %.3f kb",
                 items_info_smooth_.lights_count * sizeof(LightItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "     DECALS COUNT: %.3f", items_info_smooth_.decals_count);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "      DECALS DATA: %.3f kb",
                 items_info_smooth_.decals_count * sizeof(DecalItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "       CELLS DATA: %.3f kb",
                 ITEM_CELLS_COUNT * sizeof(CellData) / 1024.0f);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);

        vertical_offset -= font_height;
        snprintf(text_buffer, sizeof(text_buffer), "       ITEMS DATA: %.3f kb",
                 items_info_smooth_.items_total * sizeof(ItemData) / 1024.0f);
        font_->DrawText(r, text_buffer, Gui::Vec2f{-1.0f, vertical_offset}, text_color, parent_);
    }
}
