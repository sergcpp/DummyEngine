#pragma once

#include <memory>

#include <Eng/ViewerBase.h>
#include <Eng/gui/BaseElement.h>

namespace Eng {
struct BackendInfo;
struct FrontendInfo;
struct ItemsInfo;
}; // namespace Eng

class DebugInfoUI : public Gui::BaseElement {
    const Gui::BaseElement *parent_;
    const Gui::BitmapFont *font_;

    uint64_t last_frame_time_ = 0;
    double cur_frame_dur_ = 0.0;

    bool debug_items_ = false;

    struct {
        float occluders_time_ms = 0.0f, main_gather_time_ms = 0.0f, shadow_gather_time_ms = 0.0f,
              drawables_sort_time_ms = 0.0f, items_assignment_time_ms = 0.0f;
        float total_time_ms = 0.0f;
    } front_info_smooth_;

    struct {
        std::string pass_names[256];
        float pass_timings_ms[256] = {};
        int pass_timings_count = 0;

        float cpu_total_ms = 0.0f, gpu_total_ms = 0.0f;

        float shadow_draw_calls_count = 0.0f, depth_fill_draw_calls_count = 0.0f, opaque_draw_calls_count = 0.0f;

        float tris_rendered = 0.0f;
    } back_info_smooth_;

    struct {
        float lights_count = 0.0f, decals_count = 0.0f, probes_count = 0.0f;
        float items_total = 0.0f;
    } items_info_smooth_;

    struct {
        uint64_t front_start_timepoint_us = 0, front_end_timepoint_us = 0;
        uint64_t back_cpu_start_timepoint_us = 0, back_cpu_end_timepoint_us = 0;
        uint64_t back_gpu_duration = 0;
        int64_t gpu_cpu_time_diff_us = 0;
    } prev_timing_info_, cur_timing_info_;

  public:
    DebugInfoUI(const Ren::Vec2f &pos, const Ren::Vec2f &size, const BaseElement *parent, const Gui::BitmapFont *font);

    void UpdateInfo(const Eng::FrontendInfo &frontend_info, const Eng::BackendInfo &backend_info,
                    const Eng::ItemsInfo &items_info, bool debug_items);

    void Draw(Gui::Renderer *r) override;
};