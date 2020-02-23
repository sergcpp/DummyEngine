#include "DebugInfoUI.h"

#include <Eng/Gui/BitmapFont.h>
#include <Eng/Scene/SceneData.h>
#include <Eng/Renderer/Renderer_Structs.h>
#include <Sys/Time_.h>

DebugInfoUI::DebugInfoUI(const Ren::Vec2f &pos, const Ren::Vec2f &size, const BaseElement *parent, const std::shared_ptr<Gui::BitmapFont> &font)
    : BaseElement(pos, size, parent), parent_(parent), font_(font) {

}

void DebugInfoUI::UpdateInfo(const FrontendInfo &frontend_info, const BackendInfo &backend_info,
                             const ItemsInfo &items_info, const TimeInterval &swap_interval, uint32_t render_flags) {
    const float alpha = 0.98f;
    const float k = (1.0f - alpha);

    auto us_to_ms = [](uint64_t v) -> float { return 0.001f * v; };

    render_flags_ = render_flags;

    front_info_smooth_.occluders_time_ms        *= alpha;
    front_info_smooth_.occluders_time_ms        += k * us_to_ms(frontend_info.occluders_time_us);
    front_info_smooth_.main_gather_time_ms      *= alpha;
    front_info_smooth_.main_gather_time_ms      += k * us_to_ms(frontend_info.main_gather_time_us);
    front_info_smooth_.shadow_gather_time_ms    *= alpha;
    front_info_smooth_.shadow_gather_time_ms    += k * us_to_ms(frontend_info.shadow_gather_time_us);
    front_info_smooth_.drawables_sort_time_ms   *= alpha;
    front_info_smooth_.drawables_sort_time_ms   += k * us_to_ms(frontend_info.drawables_sort_time_us);
    front_info_smooth_.items_assignment_time_ms *= alpha;
    front_info_smooth_.items_assignment_time_ms += k * us_to_ms(frontend_info.items_assignment_time_us);
    front_info_smooth_.total_time_ms            *= alpha;
    front_info_smooth_.total_time_ms            += k * us_to_ms(frontend_info.end_timepoint_us - frontend_info.start_timepoint_us);

    back_info_smooth_.skinning_time_ms          *= alpha;
    back_info_smooth_.skinning_time_ms          += k * us_to_ms(backend_info.skinning_time_us);
    back_info_smooth_.vegetation_time_ms        *= alpha;
    back_info_smooth_.vegetation_time_ms        += k * us_to_ms(backend_info.vegetation_time_us);
    back_info_smooth_.shadow_time_ms            *= alpha;
    back_info_smooth_.shadow_time_ms            += k * us_to_ms(backend_info.shadow_time_us);
    back_info_smooth_.depth_opaque_pass_time_ms *= alpha;
    back_info_smooth_.depth_opaque_pass_time_ms += k * us_to_ms(backend_info.depth_opaque_pass_time_us);
    back_info_smooth_.ao_pass_time_ms           *= alpha;
    back_info_smooth_.ao_pass_time_ms           += k * us_to_ms(backend_info.ao_pass_time_us);
    back_info_smooth_.opaque_pass_time_ms       *= alpha;
    back_info_smooth_.opaque_pass_time_ms       += k * us_to_ms(backend_info.opaque_pass_time_us);
    back_info_smooth_.transp_pass_time_ms       *= alpha;
    back_info_smooth_.transp_pass_time_ms       += k * us_to_ms(backend_info.transp_pass_time_us);
    back_info_smooth_.refl_pass_time_ms         *= alpha;
    back_info_smooth_.refl_pass_time_ms         += k * us_to_ms(backend_info.refl_pass_time_us);
    back_info_smooth_.blur_pass_time_ms         *= alpha;
    back_info_smooth_.blur_pass_time_ms         += k * us_to_ms(backend_info.blur_pass_time_us);
    back_info_smooth_.blit_pass_time_ms         *= alpha;
    back_info_smooth_.blit_pass_time_ms         += k * us_to_ms(backend_info.blit_pass_time_us);
    back_info_smooth_.cpu_total_ms              *= alpha;
    back_info_smooth_.cpu_total_ms              += k * us_to_ms(backend_info.cpu_end_timepoint_us - backend_info.cpu_start_timepoint_us);
    back_info_smooth_.gpu_total_ms              *= alpha;
    back_info_smooth_.gpu_total_ms              += k * us_to_ms(backend_info.gpu_end_timepoint_us - backend_info.gpu_start_timepoint_us);
    back_info_smooth_.shadow_draw_calls_count   *= alpha;
    back_info_smooth_.shadow_draw_calls_count   += k * backend_info.shadow_draw_calls_count;
    back_info_smooth_.depth_fill_draw_calls_count *= alpha;
    back_info_smooth_.depth_fill_draw_calls_count += k * backend_info.depth_fill_draw_calls_count;
    back_info_smooth_.opaque_draw_calls_count   *= alpha;
    back_info_smooth_.opaque_draw_calls_count   += k * backend_info.opaque_draw_calls_count;
    back_info_smooth_.triangles_rendered        *= alpha;
    back_info_smooth_.triangles_rendered        += k * 0.000001f * backend_info.triangles_rendered;

    items_info_smooth_.light_sources_count      *= alpha;
    items_info_smooth_.light_sources_count      += k * items_info.light_sources_count;
    items_info_smooth_.decals_count             *= alpha;
    items_info_smooth_.decals_count             += k * items_info.decals_count;
    items_info_smooth_.probes_count             *= alpha;
    items_info_smooth_.probes_count             += k * items_info.probes_count;
    items_info_smooth_.items_total              *= alpha;
    items_info_smooth_.items_total              += k * items_info.items_total;

    prev_timing_info_ = cur_timing_info_;
    cur_timing_info_.front_start_timepoint_us   = frontend_info.start_timepoint_us;
    cur_timing_info_.front_end_timepoint_us     = frontend_info.end_timepoint_us;
    cur_timing_info_.back_cpu_start_timepoint_us = backend_info.cpu_start_timepoint_us;
    cur_timing_info_.back_cpu_end_timepoint_us  = backend_info.cpu_end_timepoint_us;
    cur_timing_info_.back_gpu_start_timepoint_us = backend_info.gpu_start_timepoint_us;
    cur_timing_info_.back_gpu_end_timepoint_us  = backend_info.gpu_end_timepoint_us;
    cur_timing_info_.gpu_cpu_time_diff_us       = backend_info.gpu_cpu_time_diff_us;
    cur_timing_info_.swap_interval              = swap_interval;
}

void DebugInfoUI::Draw(Gui::Renderer *r) {
    // Do 'immediate' drawing

    const float font_height = font_->height(parent_);

    const char delimiter[] = "------------------";
    char text_buffer[256];

    float vertical_offset = 0.75f;
    const uint8_t text_color[4] = { 255, 255, 255, 100 };

    {   // fps counter
        uint64_t cur_frame_time = Sys::GetTimeUs();

        double last_frame_dur = (cur_frame_time - last_frame_time_) * 0.000001;
        double last_frame_fps = 1.0 / last_frame_dur;

        last_frame_time_ = cur_frame_time;

        const double alpha = 0.025;
        cur_fps_ = alpha * last_frame_fps + (1.0 - alpha) * cur_fps_;

        sprintf(text_buffer, "        fps: %.1f", cur_fps_);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);
    }

    {   // renderer frontend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "   occ_rast: %.3f ms", front_info_smooth_.occluders_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "main_gather: %.3f ms", front_info_smooth_.main_gather_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "shad_gather: %.3f ms", front_info_smooth_.shadow_gather_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "    sorting: %.3f ms", front_info_smooth_.drawables_sort_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "item_assign: %.3f ms", front_info_smooth_.items_assignment_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "front_total: %.3f ms", front_info_smooth_.total_time_ms);
        font_->DrawText(r, text_buffer,Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);
    }

    {   // renderer backend performance
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " draw_calls: [%.1f, %.1f, %.1f]", back_info_smooth_.shadow_draw_calls_count,
                back_info_smooth_.depth_fill_draw_calls_count, back_info_smooth_.opaque_draw_calls_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  triangles: %.2f M", back_info_smooth_.triangles_rendered);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "   skinning: %.3f ms", back_info_smooth_.skinning_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " vegetation: %.3f ms", back_info_smooth_.vegetation_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "shadow_maps: %.3f ms", back_info_smooth_.shadow_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " depth_fill: %.3f ms", back_info_smooth_.depth_opaque_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "       ssao: %.3f ms", back_info_smooth_.ao_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "opaque_pass: %.3f ms", back_info_smooth_.opaque_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "transp_pass: %.3f ms", back_info_smooth_.transp_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  refl_pass: %.3f ms", back_info_smooth_.refl_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  blur_pass: %.3f ms", back_info_smooth_.blur_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  blit_pass: %.3f ms", back_info_smooth_.blit_pass_time_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  cpu_total: %.3f ms", back_info_smooth_.cpu_total_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "  gpu_total: %.3f ms", back_info_smooth_.gpu_total_ms);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);
    }

    if (render_flags_ & (DebugLights | DebugDecals)) {
        vertical_offset -= font_height;
        font_->DrawText(r, delimiter, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " lights_cnt: %.3f", items_info_smooth_.light_sources_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "lights_data: %.3f kb", items_info_smooth_.light_sources_count * sizeof(LightSourceItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " decals_cnt: %.3f", items_info_smooth_.decals_count);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, "decals_data: %.3f kb", items_info_smooth_.decals_count * sizeof(DecalItem) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " cells_data: %.3f kb", REN_CELLS_COUNT * sizeof(CellData) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

        vertical_offset -= font_height;
        sprintf(text_buffer, " items_data: %.3f kb", items_info_smooth_.items_total * sizeof(ItemData) / 1024.0f);
        font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);
    }

    if (render_flags_ & DebugTimings) {
        if (prev_timing_info_.front_end_timepoint_us) {
            auto prev_front_start = double(prev_timing_info_.front_start_timepoint_us),
                 prev_front_end = double(prev_timing_info_.front_end_timepoint_us),
                 prev_back_cpu_start = double(prev_timing_info_.back_cpu_start_timepoint_us),
                 prev_back_cpu_end = double(prev_timing_info_.back_cpu_end_timepoint_us),
                 prev_back_gpu_start = double(prev_timing_info_.back_gpu_start_timepoint_us),
                 prev_back_gpu_end = double(prev_timing_info_.back_gpu_end_timepoint_us),
                 prev_swap_start = double(prev_timing_info_.swap_interval.start_timepoint_us),
                 prev_swap_end = double(prev_timing_info_.swap_interval.end_timepoint_us),
                 next_front_start = double(cur_timing_info_.front_start_timepoint_us),
                 next_front_end = double(cur_timing_info_.front_end_timepoint_us),
                 next_back_cpu_start = double(cur_timing_info_.back_cpu_start_timepoint_us),
                 next_back_cpu_end = double(cur_timing_info_.back_cpu_end_timepoint_us),
                 next_back_gpu_start = double(cur_timing_info_.back_gpu_start_timepoint_us),
                 next_back_gpu_end = double(cur_timing_info_.back_gpu_end_timepoint_us),
                 next_swap_start = double(cur_timing_info_.swap_interval.start_timepoint_us),
                 next_swap_end = double(cur_timing_info_.swap_interval.end_timepoint_us);

            prev_back_gpu_start -= double(prev_timing_info_.gpu_cpu_time_diff_us);
            prev_back_gpu_end   -= double(prev_timing_info_.gpu_cpu_time_diff_us);
            next_back_gpu_start -= double(cur_timing_info_.gpu_cpu_time_diff_us);
            next_back_gpu_end   -= double(cur_timing_info_.gpu_cpu_time_diff_us);

            double start_point = prev_back_cpu_start;

            prev_front_start    -= start_point;
            prev_front_end      -= start_point;
            prev_back_cpu_start -= start_point;
            prev_back_cpu_end   -= start_point;
            prev_back_gpu_start -= start_point;
            prev_back_gpu_end   -= start_point;
            prev_swap_start     -= start_point;
            prev_swap_end       -= start_point;
            next_front_start    -= start_point;
            next_front_end      -= start_point;
            next_back_cpu_start -= start_point;
            next_back_cpu_end   -= start_point;
            next_back_gpu_start -= start_point;
            next_back_gpu_end   -= start_point;
            next_swap_start     -= start_point;
            next_swap_end       -= start_point;

            double dur = 0.0;
            int cc = 0;

            while (dur < std::max(next_front_end, next_back_gpu_end)) {
                dur += 1000000.0 / 60.0;
                cc++;
            }

            prev_front_start    /= dur;
            prev_front_end      /= dur;
            prev_back_cpu_start /= dur;
            prev_back_cpu_end   /= dur;
            prev_back_gpu_start /= dur;
            prev_back_gpu_end   /= dur;
            prev_swap_start     /= dur;
            prev_swap_end       /= dur;

            next_front_start    /= dur;
            next_front_end      /= dur;
            next_back_cpu_start /= dur;
            next_back_cpu_end   /= dur;
            next_back_gpu_start /= dur;
            next_back_gpu_end   /= dur;
            next_swap_start     /= dur;
            next_swap_end       /= dur;

            text_buffer[0] = '[';
            text_buffer[101] = ']';

            for (int i = 0; i < 100; i++) {
                double t = double(i) / 100;

                if ((t >= prev_front_start && t <= prev_front_end) ||
                    (t >= next_front_start && t <= next_front_end)) {
                    text_buffer[i + 1] = 'F';
                } else {
                    text_buffer[i + 1] = '_';
                }
            }

            sprintf(&text_buffer[102], " [2 frames, %.1f ms]", cc * 1000.0 / 60.0);

            vertical_offset -= font_height;
            font_->DrawText(r, delimiter, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

            vertical_offset -= font_height;
            font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

            for (int i = 0; i < 100; i++) {
                double t = double(i) / 100;

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
            font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);

            for (int i = 0; i < 100; i++) {
                double t = double(i) / 100;

                if ((t >= prev_back_gpu_start && t <= prev_back_gpu_end) ||
                    (t >= next_back_gpu_start && t <= next_back_gpu_end)) {
                    text_buffer[i + 1] = 'G';
                } else {
                    text_buffer[i + 1] = '_';
                }
            }

            vertical_offset -= font_height;
            font_->DrawText(r, text_buffer, Ren::Vec2f{ -1.0f, vertical_offset }, text_color, parent_);
        }
    }
}