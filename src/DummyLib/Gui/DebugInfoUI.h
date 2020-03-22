#pragma once

#include <memory>

#include <Eng/GameBase.h>
#include <Eng/Gui/BaseElement.h>

struct BackendInfo;
struct FrontendInfo;
struct ItemsInfo;

class DebugInfoUI : public Gui::BaseElement {
    const Gui::BaseElement              *parent_;
    std::shared_ptr<Gui::BitmapFont>    font_;

    uint64_t    last_frame_time_ = 0;
    double      cur_fps_ = 0.0;

    uint32_t    render_flags_ = 0;

    struct {
        float occluders_time_ms         = 0.0f,
              main_gather_time_ms       = 0.0f,
              shadow_gather_time_ms     = 0.0f,
              drawables_sort_time_ms    = 0.0f,
              items_assignment_time_ms  = 0.0f;
        float total_time_ms             = 0.0f;
    } front_info_smooth_;

    struct {
        float skinning_time_ms          = 0.0f,
              vegetation_time_ms        = 0.0f,
              shadow_time_ms            = 0.0f,
              depth_opaque_pass_time_ms = 0.0f,
              ao_pass_time_ms           = 0.0f,
              opaque_pass_time_ms       = 0.0f,
              transp_pass_time_ms       = 0.0f,
              refl_pass_time_ms         = 0.0f,
              taa_pass_time_ms          = 0.0f,
              blur_pass_time_ms         = 0.0f,
              blit_pass_time_ms         = 0.0f;

        float cpu_total_ms = 0.0f,
              gpu_total_ms = 0.0f;

        float shadow_draw_calls_count       = 0.0f,
              depth_fill_draw_calls_count   = 0.0f,
              opaque_draw_calls_count       = 0.0f;

        float triangles_rendered = 0.0f;
    } back_info_smooth_;

    struct {
        float light_sources_count   = 0.0f,
              decals_count          = 0.0f,
              probes_count          = 0.0f;
        float items_total           = 0.0f;
    } items_info_smooth_;

    struct {
        uint64_t front_start_timepoint_us   = 0,
                 front_end_timepoint_us     = 0;
        uint64_t back_cpu_start_timepoint_us = 0,
                 back_cpu_end_timepoint_us  = 0;
        uint64_t back_gpu_start_timepoint_us = 0,
                 back_gpu_end_timepoint_us  = 0;
        int64_t gpu_cpu_time_diff_us        = 0;
        TimeInterval swap_interval;
    } prev_timing_info_, cur_timing_info_;
public:
    DebugInfoUI(const Ren::Vec2f &pos, const Ren::Vec2f &size, const BaseElement *parent, const std::shared_ptr<Gui::BitmapFont> &font);

    void UpdateInfo(const FrontendInfo &frontend_info, const BackendInfo &backend_info,
                    const ItemsInfo &items_info, const TimeInterval &swap_interval, uint32_t render_flags);

    void Draw(Gui::Renderer *r) override;
};